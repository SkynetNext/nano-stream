#include <gtest/gtest.h>

#include "disruptor/BusySpinWaitStrategy.h"
#include "disruptor/EventPoller.h"
#include "disruptor/RingBuffer.h"
#include "disruptor/SleepingWaitStrategy.h"
#include "disruptor/SingleProducerSequencer.h"

#include <array>
#include <vector>

namespace {
template <typename T>
class ArrayDataProvider final : public disruptor::DataProvider<T> {
public:
  explicit ArrayDataProvider(std::array<T, 16>& data) : data_(&data) {}
  T& get(int64_t sequence) override { return (*data_)[static_cast<size_t>(sequence)]; }
private:
  std::array<T, 16>* data_;
};
} // namespace

TEST(EventPollerTest, shouldPollForEvents) {
  using WS = disruptor::BusySpinWaitStrategy;
  using Seq = disruptor::SingleProducerSequencer<WS>;
  
  disruptor::Sequence gatingSequence;
  WS waitStrategy;
  Seq sequencer(16, waitStrategy);

  std::array<void*, 16> data{};
  // DataProvider<Object> provider = sequence -> data[(int) sequence];
  class Provider final : public disruptor::DataProvider<void*> {
  public:
    explicit Provider(std::array<void*, 16>& d) : d_(&d) {}
    void*& get(int64_t sequence) override { return (*d_)[static_cast<size_t>(sequence)]; }
  private:
    std::array<void*, 16>* d_;
  } provider(data);

  disruptor::Sequence* gatingArr[1] = {&gatingSequence};
  auto sequence = std::make_shared<disruptor::Sequence>(0);
  auto& cursorSequence = sequencer.cursorSequence();
  auto poller = disruptor::EventPoller<void*, Seq>::newInstance(provider, sequencer, sequence, cursorSequence, gatingArr, 1);

  using PollerT = disruptor::EventPoller<void*, Seq>;
  struct Handler final : public PollerT::Handler {
    bool onEvent(void*& /*event*/, int64_t /*sequence*/, bool /*endOfBatch*/) override { return false; }
  } handler;

  int x = 1;
  data[0] = &x;

  EXPECT_EQ(PollerT::PollState::IDLE, poller->poll(handler));

  sequencer.publish(sequencer.next());
  EXPECT_EQ(PollerT::PollState::GATING, poller->poll(handler));

  gatingSequence.incrementAndGet();
  EXPECT_EQ(PollerT::PollState::PROCESSING, poller->poll(handler));
}

TEST(EventPollerTest, shouldSuccessfullyPollWhenBufferIsFull) {
  using Event = std::array<uint8_t, 1>;
  using WS = disruptor::SleepingWaitStrategy;
  using RB = disruptor::MultiProducerRingBuffer<Event, WS>;
  
  std::vector<Event> events;

  WS ws;
  struct Factory final : public disruptor::EventFactory<Event> {
    Event newInstance() override { return Event{0}; }
  };
  auto ringBuffer = RB::createMultiProducer(std::make_shared<Factory>(), 4, ws);

  using PollerT = disruptor::EventPoller<Event, typename RB::SequencerType>;
  struct Handler final : public PollerT::Handler {
    explicit Handler(std::vector<Event>& events) : events_(&events) {}
    bool onEvent(Event& event, int64_t /*sequence*/, bool endOfBatch) override {
      events_->push_back(event);
      return !endOfBatch;
    }
    std::vector<Event>* events_;
  } handler(events);

  auto poller = ringBuffer->newPoller();
  ringBuffer->addGatingSequences(poller->getSequence());

  for (uint8_t i = 1; i <= 4; ++i) {
    const int64_t next = ringBuffer->next();
    ringBuffer->get(next)[0] = i;
    ringBuffer->publish(next);
  }

  poller->poll(handler);
  EXPECT_EQ(4u, events.size());
}
