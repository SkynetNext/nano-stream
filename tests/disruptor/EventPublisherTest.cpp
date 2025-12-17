#include <gtest/gtest.h>

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
  auto ringBuffer =
      disruptor::RingBuffer<disruptor::support::LongEvent>::createMultiProducer(disruptor::support::LongEvent::FACTORY, BUFFER_SIZE);

  disruptor::NoOpEventProcessor<disruptor::support::LongEvent> noop(*ringBuffer);
  ringBuffer->addGatingSequences(noop.getSequence());

  EventPublisherTestTranslator translator;
  ringBuffer->publishEvent(translator);
  ringBuffer->publishEvent(translator);

  EXPECT_EQ(0 + 29, ringBuffer->get(0).get());
  EXPECT_EQ(1 + 29, ringBuffer->get(1).get());
}

TEST(EventPublisherTest, shouldTryPublishEvent) {
  auto ringBuffer =
      disruptor::RingBuffer<disruptor::support::LongEvent>::createMultiProducer(disruptor::support::LongEvent::FACTORY, BUFFER_SIZE);

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
