#include <gtest/gtest.h>

#include "disruptor/BatchEventProcessorBuilder.h"
#include "disruptor/BusySpinWaitStrategy.h"
#include "disruptor/ExceptionHandler.h"
#include "disruptor/RingBuffer.h"
#include "tests/disruptor/support/StubEvent.h"
#include "tests/disruptor/test_support/CountDownLatch.h"

#include <stdexcept>
#include <thread>

namespace {
class LatchEventHandler final : public disruptor::EventHandler<disruptor::support::StubEvent> {
public:
  explicit LatchEventHandler(disruptor::test_support::CountDownLatch& latch) : latch_(&latch) {}
  void onEvent(disruptor::support::StubEvent& /*event*/, int64_t /*sequence*/, bool /*endOfBatch*/) override {
    latch_->countDown();
  }
private:
  disruptor::test_support::CountDownLatch* latch_;
};

class ExceptionEventHandler final : public disruptor::EventHandler<disruptor::support::StubEvent> {
public:
  void onEvent(disruptor::support::StubEvent& /*event*/, int64_t /*sequence*/, bool /*endOfBatch*/) override {
    throw std::runtime_error("boom");
  }
};

class LatchExceptionHandler final : public disruptor::ExceptionHandler<disruptor::support::StubEvent> {
public:
  explicit LatchExceptionHandler(disruptor::test_support::CountDownLatch& latch) : latch_(&latch) {}
  void handleEventException(const std::exception& /*ex*/, int64_t /*sequence*/, disruptor::support::StubEvent* /*event*/) override {
    latch_->countDown();
  }
  void handleOnStartException(const std::exception& /*ex*/) override {}
  void handleOnShutdownException(const std::exception& /*ex*/) override {}
private:
  disruptor::test_support::CountDownLatch* latch_;
};
} // namespace

TEST(BatchEventProcessorTest, shouldCallMethodsInLifecycleOrderForBatch) {
  using Event = disruptor::support::StubEvent;
  using WS = disruptor::BusySpinWaitStrategy;
  using RB = disruptor::MultiProducerRingBuffer<Event, WS>;
  WS ws;
  auto ringBuffer = RB::createMultiProducer(disruptor::support::StubEvent::EVENT_FACTORY, 16, ws);
  auto barrier = ringBuffer->newBarrier(nullptr, 0);
  disruptor::test_support::CountDownLatch latch(3);
  LatchEventHandler handler(latch);
  disruptor::BatchEventProcessorBuilder builder;
  auto processor = builder.build(*ringBuffer, *barrier, handler);
  ringBuffer->addGatingSequences(processor->getSequence());

  ringBuffer->publish(ringBuffer->next());
  ringBuffer->publish(ringBuffer->next());
  ringBuffer->publish(ringBuffer->next());

  std::thread t([&] { processor->run(); });
  latch.await();
  processor->halt();
  t.join();
}

TEST(BatchEventProcessorTest, shouldCallExceptionHandlerOnUncaughtException) {
  using Event = disruptor::support::StubEvent;
  using WS = disruptor::BusySpinWaitStrategy;
  using RB = disruptor::MultiProducerRingBuffer<Event, WS>;
  WS ws;
  auto ringBuffer = RB::createMultiProducer(disruptor::support::StubEvent::EVENT_FACTORY, 16, ws);
  auto barrier = ringBuffer->newBarrier(nullptr, 0);
  disruptor::test_support::CountDownLatch latch(1);
  ExceptionEventHandler handler;
  disruptor::BatchEventProcessorBuilder builder;
  auto processor = builder.build(*ringBuffer, *barrier, handler);
  ringBuffer->addGatingSequences(processor->getSequence());

  LatchExceptionHandler exHandler(latch);
  processor->setExceptionHandler(exHandler);

  std::thread t([&] { processor->run(); });
  ringBuffer->publish(ringBuffer->next());
  latch.await();
  processor->halt();
  t.join();
}
