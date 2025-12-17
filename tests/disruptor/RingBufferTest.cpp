#include <gtest/gtest.h>

#include "disruptor/NoOpEventProcessor.h"
#include "disruptor/RingBuffer.h"
#include "disruptor/Sequence.h"
#include "tests/disruptor/support/StubEvent.h"

#include <string>

TEST(RingBufferTest, shouldClaimAndGet) {
  auto ringBuffer = disruptor::RingBuffer<disruptor::support::StubEvent>::createMultiProducer(disruptor::support::StubEvent::EVENT_FACTORY, 32);
  auto barrier = ringBuffer->newBarrier(nullptr, 0);

  disruptor::NoOpEventProcessor<disruptor::support::StubEvent> noop(*ringBuffer);
  ringBuffer->addGatingSequences(noop.getSequence());

  EXPECT_EQ(disruptor::SingleProducerSequencer::INITIAL_CURSOR_VALUE, ringBuffer->getCursor());

  ringBuffer->publishEvent(disruptor::support::StubEvent::TRANSLATOR, 2701, std::string{});
  int64_t seq = barrier->waitFor(0);
  EXPECT_EQ(0, seq);
  EXPECT_EQ(2701, ringBuffer->get(seq).getValue());
  EXPECT_EQ(0, ringBuffer->getCursor());
}

TEST(RingBufferTest, shouldPreventWrapping) {
  auto rb = disruptor::RingBuffer<disruptor::support::StubEvent>::createMultiProducer(disruptor::support::StubEvent::EVENT_FACTORY, 4);
  disruptor::Sequence gating(disruptor::Sequencer::INITIAL_CURSOR_VALUE);
  rb->addGatingSequences(gating);

  rb->publishEvent(disruptor::support::StubEvent::TRANSLATOR, 0, std::string{"0"});
  rb->publishEvent(disruptor::support::StubEvent::TRANSLATOR, 1, std::string{"1"});
  rb->publishEvent(disruptor::support::StubEvent::TRANSLATOR, 2, std::string{"2"});
  rb->publishEvent(disruptor::support::StubEvent::TRANSLATOR, 3, std::string{"3"});

  EXPECT_FALSE(rb->tryPublishEvent(disruptor::support::StubEvent::TRANSLATOR, 3, std::string{"3"}));
}

TEST(RingBufferTest, shouldThrowExceptionIfBufferIsFull) {
  auto ringBuffer = disruptor::RingBuffer<disruptor::support::StubEvent>::createMultiProducer(disruptor::support::StubEvent::EVENT_FACTORY, 32);
  auto barrier = ringBuffer->newBarrier(nullptr, 0);
  (void)barrier;

  // Gate behind cursor so buffer is full.
  disruptor::Sequence gating(ringBuffer->getBufferSize());
  ringBuffer->addGatingSequences(gating);
  for (int i = 0; i < ringBuffer->getBufferSize(); ++i) {
    ringBuffer->publish(ringBuffer->tryNext());
  }
  EXPECT_THROW((void)ringBuffer->tryNext(), disruptor::InsufficientCapacityException);
}
