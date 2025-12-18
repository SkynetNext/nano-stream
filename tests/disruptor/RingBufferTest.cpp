#include <gtest/gtest.h>

#include "disruptor/BusySpinWaitStrategy.h"
#include "disruptor/NoOpEventProcessor.h"
#include "disruptor/RingBuffer.h"
#include "disruptor/Sequence.h"
#include "tests/disruptor/support/StubEvent.h"

#include <string>

TEST(RingBufferTest, shouldClaimAndGet) {
  using Event = disruptor::support::StubEvent;
  using WS = disruptor::BusySpinWaitStrategy;
  WS ws;
  auto ringBuffer = disruptor::MultiProducerRingBuffer<Event, WS>::createMultiProducer(disruptor::support::StubEvent::EVENT_FACTORY, 32, ws);
  auto barrier = ringBuffer->newBarrier(nullptr, 0);

  using RB = std::remove_reference_t<decltype(*ringBuffer)>;
  disruptor::NoOpEventProcessor<Event, RB> noop(*ringBuffer);
  ringBuffer->addGatingSequences(noop.getSequence());

  EXPECT_EQ(disruptor::Sequencer::INITIAL_CURSOR_VALUE, ringBuffer->getCursor());

  ringBuffer->publishEvent(disruptor::support::StubEvent::TRANSLATOR, 2701, std::string{});
  int64_t seq = barrier->waitFor(0);
  EXPECT_EQ(0, seq);
  EXPECT_EQ(2701, ringBuffer->get(seq).getValue());
  EXPECT_EQ(0, ringBuffer->getCursor());
}

TEST(RingBufferTest, shouldPreventWrapping) {
  using Event = disruptor::support::StubEvent;
  using WS = disruptor::BusySpinWaitStrategy;
  WS ws;
  auto rb = disruptor::MultiProducerRingBuffer<Event, WS>::createMultiProducer(disruptor::support::StubEvent::EVENT_FACTORY, 4, ws);
  disruptor::Sequence gating(disruptor::Sequencer::INITIAL_CURSOR_VALUE);
  rb->addGatingSequences(gating);

  rb->publishEvent(disruptor::support::StubEvent::TRANSLATOR, 0, std::string{"0"});
  rb->publishEvent(disruptor::support::StubEvent::TRANSLATOR, 1, std::string{"1"});
  rb->publishEvent(disruptor::support::StubEvent::TRANSLATOR, 2, std::string{"2"});
  rb->publishEvent(disruptor::support::StubEvent::TRANSLATOR, 3, std::string{"3"});

  EXPECT_FALSE(rb->tryPublishEvent(disruptor::support::StubEvent::TRANSLATOR, 3, std::string{"3"}));
}

TEST(RingBufferTest, shouldThrowExceptionIfBufferIsFull) {
  using Event = disruptor::support::StubEvent;
  using WS = disruptor::BusySpinWaitStrategy;
  WS ws;
  auto ringBuffer = disruptor::MultiProducerRingBuffer<Event, WS>::createMultiProducer(disruptor::support::StubEvent::EVENT_FACTORY, 32, ws);
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
