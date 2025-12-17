#include <gtest/gtest.h>

#include "disruptor/BatchEventProcessor.h"
#include "disruptor/BatchEventProcessorBuilder.h"
#include "disruptor/RingBuffer.h"
#include "tests/disruptor/support/StubEvent.h"
#include "tests/disruptor/test_support/CountDownLatch.h"

#include <thread>

namespace {
class TestSequenceReportingEventHandler final : public disruptor::EventHandler<disruptor::support::StubEvent> {
public:
  TestSequenceReportingEventHandler(disruptor::test_support::CountDownLatch& callbackLatch,
                                   disruptor::test_support::CountDownLatch& onEndOfBatchLatch)
      : callbackLatch_(&callbackLatch), onEndOfBatchLatch_(&onEndOfBatchLatch) {}

  void setSequenceCallback(disruptor::Sequence& sequenceTrackerCallback) override {
    sequenceCallback_ = &sequenceTrackerCallback;
  }

  void onEvent(disruptor::support::StubEvent& /*event*/, int64_t sequence, bool endOfBatch) override {
    sequenceCallback_->set(sequence);
    callbackLatch_->countDown();

    if (endOfBatch) {
      onEndOfBatchLatch_->await();
    }
  }

private:
  disruptor::Sequence* sequenceCallback_{nullptr};
  disruptor::test_support::CountDownLatch* callbackLatch_;
  disruptor::test_support::CountDownLatch* onEndOfBatchLatch_;
};
} // namespace

TEST(SequenceReportingCallbackTest, shouldReportProgressByUpdatingSequenceViaCallback) {
  disruptor::test_support::CountDownLatch callbackLatch(1);
  disruptor::test_support::CountDownLatch onEndOfBatchLatch(1);

  auto ringBuffer =
      disruptor::RingBuffer<disruptor::support::StubEvent>::createMultiProducer(disruptor::support::StubEvent::EVENT_FACTORY, 16);
  auto sequenceBarrier = ringBuffer->newBarrier(nullptr, 0);

  TestSequenceReportingEventHandler handler(callbackLatch, onEndOfBatchLatch);
  disruptor::BatchEventProcessorBuilder builder;
  auto batchEventProcessor = builder.build(*ringBuffer, *sequenceBarrier, handler);

  ringBuffer->addGatingSequences(batchEventProcessor->getSequence());

  std::thread t([&] { batchEventProcessor->run(); });

  EXPECT_EQ(-1, batchEventProcessor->getSequence().get());
  ringBuffer->publish(ringBuffer->next());

  callbackLatch.await();
  EXPECT_EQ(0, batchEventProcessor->getSequence().get());

  onEndOfBatchLatch.countDown();
  EXPECT_EQ(0, batchEventProcessor->getSequence().get());

  batchEventProcessor->halt();
  t.join();
}
