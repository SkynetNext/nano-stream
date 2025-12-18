#include <gtest/gtest.h>

#include "disruptor/BlockingWaitStrategy.h"
#include "disruptor/BusySpinWaitStrategy.h"
#include "disruptor/MultiProducerSequencer.h"
#include "disruptor/SingleProducerSequencer.h"
#include "disruptor/dsl/ProducerType.h"
#include "tests/disruptor/support/DummyWaitStrategy.h"
#include "tests/disruptor/test_support/CountDownLatch.h"

#include <memory>
#include <thread>

namespace {
static constexpr int BUFFER_SIZE = 16;

template <typename WS>
auto newSingleProducer(int bufferSize, WS& ws) {
  return std::make_unique<disruptor::SingleProducerSequencer<WS>>(bufferSize, ws);
}

template <typename WS>
auto newMultiProducer(int bufferSize, WS& ws) {
  return std::make_unique<disruptor::MultiProducerSequencer<WS>>(bufferSize, ws);
}
} // namespace

TEST(SequencerTest, shouldStartWithInitialValue_single) {
  using WS = disruptor::BlockingWaitStrategy;
  WS ws;
  auto s = newSingleProducer(BUFFER_SIZE, ws);
  EXPECT_EQ(0, s->next());
}

TEST(SequencerTest, shouldStartWithInitialValue_multi) {
  using WS = disruptor::BlockingWaitStrategy;
  WS ws;
  auto s = newMultiProducer(BUFFER_SIZE, ws);
  EXPECT_EQ(0, s->next());
}

TEST(SequencerTest, shouldBatchClaim) {
  using WS = disruptor::BlockingWaitStrategy;
  WS ws;
  auto s = newSingleProducer(BUFFER_SIZE, ws);
  EXPECT_EQ(3, s->next(4));
}

TEST(SequencerTest, shouldNotifyWaitStrategyOnPublish) {
  using WS = disruptor::support::DummyWaitStrategy;
  WS ws;
  auto s = newSingleProducer(BUFFER_SIZE, ws);
  s->publish(s->next());
  EXPECT_EQ(ws.signalAllWhenBlockingCalls, 1);
}
