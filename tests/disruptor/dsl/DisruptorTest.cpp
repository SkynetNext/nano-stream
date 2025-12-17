#include <gtest/gtest.h>

#include "disruptor/BlockingWaitStrategy.h"
#include "disruptor/dsl/Disruptor.h"
#include "disruptor/dsl/ProducerType.h"
#include "disruptor/util/DaemonThreadFactory.h"
#include "tests/disruptor/support/TestEvent.h"
#include "tests/disruptor/test_support/CountDownLatch.h"

#include <memory>

namespace {
class LatchHandler final : public disruptor::EventHandler<disruptor::support::TestEvent> {
public:
  explicit LatchHandler(disruptor::test_support::CountDownLatch& latch) : latch_(&latch) {}
  void onEvent(disruptor::support::TestEvent& /*event*/, int64_t /*sequence*/, bool /*endOfBatch*/) override {
    latch_->countDown();
  }
private:
  disruptor::test_support::CountDownLatch* latch_;
};

class NoOpTranslator final : public disruptor::EventTranslator<disruptor::support::TestEvent> {
public:
  void translateTo(disruptor::support::TestEvent& /*event*/, int64_t /*sequence*/) override {}
};
} // namespace

TEST(DisruptorTest, shouldHaveStartedAfterStartCalled) {
  auto& tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  auto ws = std::make_unique<disruptor::BlockingWaitStrategy>();
  disruptor::dsl::Disruptor<disruptor::support::TestEvent> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, disruptor::dsl::ProducerType::MULTI, std::move(ws));

  EXPECT_FALSE(d.hasStarted());
  d.start();
  EXPECT_TRUE(d.hasStarted());
  d.halt();
}

TEST(DisruptorTest, shouldProcessMessagesPublishedBeforeStartIsCalled) {
  auto& tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  auto ws = std::make_unique<disruptor::BlockingWaitStrategy>();
  disruptor::dsl::Disruptor<disruptor::support::TestEvent> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, disruptor::dsl::ProducerType::MULTI, std::move(ws));

  disruptor::test_support::CountDownLatch latch(2);
  LatchHandler handler(latch);
  d.handleEventsWith(handler);

  NoOpTranslator translator;
  d.publishEvent(translator); // before start
  d.start();
  d.publishEvent(translator); // after start

  latch.await();
  d.halt();
}
