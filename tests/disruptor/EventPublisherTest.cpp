#include <gtest/gtest.h>

#include "disruptor/BusySpinWaitStrategy.h"
#include "disruptor/NoOpEventProcessor.h"
#include "disruptor/RingBuffer.h"
#include "disruptor/Sequence.h"
#include "tests/disruptor/support/LongEvent.h"

namespace {
static constexpr int BUFFER_SIZE = 32;

class EventPublisherTestTranslator final : public disruptor::EventTranslator<disruptor::support::LongEvent> {
public:
  void translateTo(disruptor::support::LongEvent& event, int64_t sequence) override { event.set(sequence + 29); }
};
} // namespace

TEST(EventPublisherTest, shouldPublishEvent) {
  using Event = disruptor::support::LongEvent;
  using WS = disruptor::BusySpinWaitStrategy;
  using RB = disruptor::MultiProducerRingBuffer<Event, WS>;
  WS ws;
  auto ringBuffer = RB::createMultiProducer(disruptor::support::LongEvent::FACTORY, BUFFER_SIZE, ws);

  disruptor::NoOpEventProcessor<Event, RB> noop(*ringBuffer);
  ringBuffer->addGatingSequences(noop.getSequence());

  EventPublisherTestTranslator translator;
  ringBuffer->publishEvent(translator);
  ringBuffer->publishEvent(translator);

  EXPECT_EQ(0 + 29, ringBuffer->get(0).get());
  EXPECT_EQ(1 + 29, ringBuffer->get(1).get());
}

TEST(EventPublisherTest, shouldTryPublishEvent) {
  using Event = disruptor::support::LongEvent;
  using WS = disruptor::BusySpinWaitStrategy;
  using RB = disruptor::MultiProducerRingBuffer<Event, WS>;
  WS ws;
  auto ringBuffer = RB::createMultiProducer(disruptor::support::LongEvent::FACTORY, BUFFER_SIZE, ws);

  disruptor::Sequence gating;
  ringBuffer->addGatingSequences(gating);

  EventPublisherTestTranslator translator;
  for (int i = 0; i < BUFFER_SIZE; ++i) {
    EXPECT_TRUE(ringBuffer->tryPublishEvent(translator));
  }

  for (int i = 0; i < BUFFER_SIZE; ++i) {
    EXPECT_EQ(i + 29, ringBuffer->get(i).get());
  }

  EXPECT_FALSE(ringBuffer->tryPublishEvent(translator));
}
