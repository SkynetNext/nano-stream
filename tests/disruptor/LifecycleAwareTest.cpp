#include <gtest/gtest.h>

#include "disruptor/BatchEventProcessor.h"
#include "disruptor/BatchEventProcessorBuilder.h"
#include "disruptor/RingBuffer.h"
#include "tests/disruptor/support/StubEvent.h"
#include "tests/disruptor/test_support/CountDownLatch.h"

#include <thread>

namespace {
class LifecycleAwareEventHandler final : public disruptor::EventHandler<disruptor::support::StubEvent> {
public:
  explicit LifecycleAwareEventHandler(disruptor::test_support::CountDownLatch& startLatch,
                                      disruptor::test_support::CountDownLatch& shutdownLatch)
      : startLatch_(&startLatch), shutdownLatch_(&shutdownLatch) {}

  void onEvent(disruptor::support::StubEvent& /*event*/, int64_t /*sequence*/, bool /*endOfBatch*/) override {}

  void onStart() override {
    ++startCounter;
    startLatch_->countDown();
  }

  void onShutdown() override {
    ++shutdownCounter;
    shutdownLatch_->countDown();
  }

  int startCounter = 0;
  int shutdownCounter = 0;

private:
  disruptor::test_support::CountDownLatch* startLatch_;
  disruptor::test_support::CountDownLatch* shutdownLatch_;
};
} // namespace

TEST(LifecycleAwareTest, shouldNotifyOfBatchProcessorLifecycle) {
  disruptor::test_support::CountDownLatch startLatch(1);
  disruptor::test_support::CountDownLatch shutdownLatch(1);

  auto ringBuffer =
      disruptor::RingBuffer<disruptor::support::StubEvent>::createMultiProducer(disruptor::support::StubEvent::EVENT_FACTORY, 16);
  auto sequenceBarrier = ringBuffer->newBarrier(nullptr, 0);

  LifecycleAwareEventHandler handler(startLatch, shutdownLatch);
  disruptor::BatchEventProcessorBuilder builder;
  auto batchEventProcessor = builder.build(*ringBuffer, *sequenceBarrier, handler);

  std::thread t([&] { batchEventProcessor->run(); });

  startLatch.await();
  batchEventProcessor->halt();
  shutdownLatch.await();

  EXPECT_EQ(1, handler.startCounter);
  EXPECT_EQ(1, handler.shutdownCounter);

  t.join();
}
