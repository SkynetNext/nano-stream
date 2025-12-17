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
  disruptor::Sequence gatingSequence;
  auto waitStrategy = std::make_unique<disruptor::BusySpinWaitStrategy>();
  disruptor::SingleProducerSequencer sequencer(16, std::move(waitStrategy));

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
  auto poller = sequencer.newPoller(provider, gatingArr, 1);

  struct Handler final : public disruptor::EventPoller<void*>::Handler {
    bool onEvent(void*& /*event*/, int64_t /*sequence*/, bool /*endOfBatch*/) override { return false; }
  } handler;

  int x = 1;
  data[0] = &x;

  EXPECT_EQ(disruptor::EventPoller<void*>::PollState::IDLE, poller->poll(handler));

  sequencer.publish(sequencer.next());
  EXPECT_EQ(disruptor::EventPoller<void*>::PollState::GATING, poller->poll(handler));

  gatingSequence.incrementAndGet();
  EXPECT_EQ(disruptor::EventPoller<void*>::PollState::PROCESSING, poller->poll(handler));
}

TEST(EventPollerTest, shouldSuccessfullyPollWhenBufferIsFull) {
  std::vector<std::array<uint8_t, 1>> events;

  struct Handler final : public disruptor::EventPoller<std::array<uint8_t, 1>>::Handler {
    explicit Handler(std::vector<std::array<uint8_t, 1>>& events) : events_(&events) {}
    bool onEvent(std::array<uint8_t, 1>& event, int64_t /*sequence*/, bool endOfBatch) override {
      events_->push_back(event);
      return !endOfBatch;
    }
    std::vector<std::array<uint8_t, 1>>* events_;
  } handler(events);

  struct Factory final : public disruptor::EventFactory<std::array<uint8_t, 1>> {
    std::array<uint8_t, 1> newInstance() override { return std::array<uint8_t, 1>{0}; }
  };

  auto ringBuffer = disruptor::RingBuffer<std::array<uint8_t, 1>>::createMultiProducer(
      std::make_shared<Factory>(), 4, std::make_unique<disruptor::SleepingWaitStrategy>());

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
