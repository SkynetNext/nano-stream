#include <gtest/gtest.h>

#include "disruptor/SleepingWaitStrategy.h"
#include "disruptor/dsl/Disruptor.h"
#include "disruptor/util/DaemonThreadFactory.h"
#include "tests/disruptor/support/LongEvent.h"

#include <chrono>
#include <thread>

namespace {
class ParallelEventHandler final : public disruptor::EventHandler<disruptor::support::LongEvent> {
public:
  ParallelEventHandler(int64_t mask, int64_t ordinal) : mask_(mask), ordinal_(ordinal) {}

  void onEvent(disruptor::support::LongEvent& event, int64_t sequence, bool endOfBatch) override {
    if ((sequence & mask_) == ordinal_) {
      ++eventCount;
      tempValue = event.get();
    }

    if (endOfBatch || ++batchCount >= batchSize) {
      publishedValue = tempValue;
      batchCount = 0;
    } else {
      std::this_thread::yield();
    }

    processed.store(sequence, std::memory_order_release);
  }

  const int batchSize = 10;
  int64_t eventCount = 0;
  int64_t batchCount = 0;
  int64_t publishedValue = 0;
  int64_t tempValue = 0;
  std::atomic<int64_t> processed{-1};

private:
  int64_t mask_;
  int64_t ordinal_;
};

class SetSequenceTranslator final : public disruptor::EventTranslator<disruptor::support::LongEvent> {
public:
  void translateTo(disruptor::support::LongEvent& event, int64_t sequence) override { event.set(sequence); }
};
} // namespace

class BatchingTestFixture : public ::testing::TestWithParam<disruptor::dsl::ProducerType> {};

TEST_P(BatchingTestFixture, shouldBatch) {
  const auto producerType = GetParam();

  auto waitStrategy = std::make_unique<disruptor::SleepingWaitStrategy>();
  auto& threadFactory = disruptor::util::DaemonThreadFactory::INSTANCE();

  disruptor::dsl::Disruptor<disruptor::support::LongEvent> d(
      disruptor::support::LongEvent::FACTORY, 2048, threadFactory, producerType, std::move(waitStrategy));

  ParallelEventHandler handler1(1, 0);
  ParallelEventHandler handler2(1, 1);
  d.handleEventsWith(handler1, handler2);

  auto buffer = d.start();

  SetSequenceTranslator translator;
  constexpr int eventCount = 10000;
  for (int i = 0; i < eventCount; ++i) {
    buffer->publishEvent(translator);
  }

  while (handler1.processed.load(std::memory_order_acquire) != eventCount - 1 ||
         handler2.processed.load(std::memory_order_acquire) != eventCount - 1) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  EXPECT_EQ(eventCount - 2, handler1.publishedValue);
  EXPECT_EQ(eventCount / 2, handler1.eventCount);
  EXPECT_EQ(eventCount - 1, handler2.publishedValue);
  EXPECT_EQ(eventCount / 2, handler2.eventCount);
}

INSTANTIATE_TEST_SUITE_P(BatchingTest, BatchingTestFixture,
                         ::testing::Values(disruptor::dsl::ProducerType::MULTI, disruptor::dsl::ProducerType::SINGLE));
