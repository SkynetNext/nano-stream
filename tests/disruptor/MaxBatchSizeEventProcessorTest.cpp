#include <gtest/gtest.h>

#include "disruptor/BatchEventProcessorBuilder.h"
#include "disruptor/RingBuffer.h"
#include "tests/disruptor/support/StubEvent.h"
#include "tests/disruptor/test_support/CountDownLatch.h"

#include <thread>
#include <vector>

namespace {
static constexpr int MAX_BATCH_SIZE = 3;
static constexpr int PUBLISH_COUNT = 5;

class BatchLimitRecordingHandler final : public disruptor::EventHandler<disruptor::support::StubEvent> {
public:
  explicit BatchLimitRecordingHandler(disruptor::test_support::CountDownLatch& latch) : latch_(&latch) {}

  void onBatchStart(int64_t batchSize, int64_t queueDepth) override {
    currentSequences.clear();
    announcedBatchSizes.push_back(batchSize);
    announcedQueueDepths.push_back(queueDepth);
  }

  void onEvent(disruptor::support::StubEvent& /*event*/, int64_t sequence, bool endOfBatch) override {
    currentSequences.push_back(sequence);
    if (endOfBatch) {
      batchedSequences.push_back(currentSequences);
    }
    latch_->countDown();
  }

  std::vector<std::vector<int64_t>> batchedSequences;
  std::vector<int64_t> announcedBatchSizes;
  std::vector<int64_t> announcedQueueDepths;

private:
  disruptor::test_support::CountDownLatch* latch_;
  std::vector<int64_t> currentSequences;
};
} // namespace

TEST(MaxBatchSizeEventProcessorTest, shouldLimitTheBatchToConfiguredMaxBatchSize) {
  auto ringBuffer =
      disruptor::RingBuffer<disruptor::support::StubEvent>::createSingleProducer(disruptor::support::StubEvent::EVENT_FACTORY, 16);
  auto sequenceBarrier = ringBuffer->newBarrier(nullptr, 0);

  disruptor::test_support::CountDownLatch latch(PUBLISH_COUNT);
  BatchLimitRecordingHandler handler(latch);

  disruptor::BatchEventProcessorBuilder builder;
  builder.setMaxBatchSize(MAX_BATCH_SIZE);
  auto processor = builder.build(*ringBuffer, *sequenceBarrier, handler);
  ringBuffer->addGatingSequences(processor->getSequence());

  std::thread t([&] { processor->run(); });

  int64_t hi = -1;
  for (int i = 0; i < PUBLISH_COUNT; ++i) {
    hi = ringBuffer->next();
  }
  ringBuffer->publish(hi);

  latch.await();
  processor->halt();
  t.join();

  ASSERT_EQ(handler.batchedSequences.size(), 2u);
  EXPECT_EQ(handler.batchedSequences[0], (std::vector<int64_t>{0, 1, 2}));
  EXPECT_EQ(handler.batchedSequences[1], (std::vector<int64_t>{3, 4}));
}

TEST(MaxBatchSizeEventProcessorTest, shouldAnnounceBatchSizeAndQueueDepthAtTheStartOfBatch) {
  auto ringBuffer =
      disruptor::RingBuffer<disruptor::support::StubEvent>::createSingleProducer(disruptor::support::StubEvent::EVENT_FACTORY, 16);
  auto sequenceBarrier = ringBuffer->newBarrier(nullptr, 0);

  disruptor::test_support::CountDownLatch latch(PUBLISH_COUNT);
  BatchLimitRecordingHandler handler(latch);

  disruptor::BatchEventProcessorBuilder builder;
  builder.setMaxBatchSize(MAX_BATCH_SIZE);
  auto processor = builder.build(*ringBuffer, *sequenceBarrier, handler);
  ringBuffer->addGatingSequences(processor->getSequence());

  std::thread t([&] { processor->run(); });

  int64_t hi = -1;
  for (int i = 0; i < PUBLISH_COUNT; ++i) {
    hi = ringBuffer->next();
  }
  ringBuffer->publish(hi);

  latch.await();
  processor->halt();
  t.join();

  EXPECT_EQ(handler.announcedBatchSizes, (std::vector<int64_t>{3, 2}));
  EXPECT_EQ(handler.announcedQueueDepths, (std::vector<int64_t>{5, 2}));
}
