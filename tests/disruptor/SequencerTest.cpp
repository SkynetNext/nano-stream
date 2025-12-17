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

std::unique_ptr<disruptor::Sequencer> newProducer(disruptor::dsl::ProducerType producerType,
                                                 std::shared_ptr<disruptor::WaitStrategy> waitStrategy) {
  switch (producerType) {
    case disruptor::dsl::ProducerType::SINGLE:
      return std::make_unique<disruptor::SingleProducerSequencer>(BUFFER_SIZE, std::move(waitStrategy));
    case disruptor::dsl::ProducerType::MULTI:
      return std::make_unique<disruptor::MultiProducerSequencer>(BUFFER_SIZE, std::move(waitStrategy));
    default:
      throw std::runtime_error("bad producer type");
  }
}
} // namespace

TEST(SequencerTest, shouldStartWithInitialValue_single) {
  auto s = newProducer(disruptor::dsl::ProducerType::SINGLE, std::make_shared<disruptor::BlockingWaitStrategy>());
  EXPECT_EQ(0, s->next());
}

TEST(SequencerTest, shouldStartWithInitialValue_multi) {
  auto s = newProducer(disruptor::dsl::ProducerType::MULTI, std::make_shared<disruptor::BlockingWaitStrategy>());
  EXPECT_EQ(0, s->next());
}

TEST(SequencerTest, shouldBatchClaim) {
  auto s = newProducer(disruptor::dsl::ProducerType::SINGLE, std::make_shared<disruptor::BlockingWaitStrategy>());
  EXPECT_EQ(3, s->next(4));
}

TEST(SequencerTest, shouldNotifyWaitStrategyOnPublish) {
  auto ws = std::make_shared<disruptor::support::DummyWaitStrategy>();
  auto s = newProducer(disruptor::dsl::ProducerType::SINGLE, ws);
  s->publish(s->next());
  EXPECT_EQ(ws->signalAllWhenBlockingCalls, 1);
}
