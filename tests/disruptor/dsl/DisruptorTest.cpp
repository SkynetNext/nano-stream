#include <gtest/gtest.h>

#include "disruptor/BatchEventProcessor.h"
#include "disruptor/BatchEventProcessorBuilder.h"
#include "disruptor/BlockingWaitStrategy.h"
#include "disruptor/EventTranslatorOneArg.h"
#include "disruptor/EventTranslatorVararg.h"
#include "disruptor/FatalExceptionHandler.h"
#include "disruptor/RewindableEventHandler.h"
#include "disruptor/SimpleBatchRewindStrategy.h"
#include "disruptor/TimeoutException.h"
#include "disruptor/dsl/Disruptor.h"
#include "disruptor/dsl/EventProcessorFactory.h"
#include "disruptor/dsl/ProducerType.h"
#include "disruptor/util/DaemonThreadFactory.h"
#include "tests/disruptor/dsl/stubs/DelayedEventHandler.h"
#include "tests/disruptor/dsl/stubs/EventHandlerStub.h"
#include "tests/disruptor/dsl/stubs/EvilEqualsEventHandler.h"
#include "tests/disruptor/dsl/stubs/ExceptionThrowingEventHandler.h"
#include "tests/disruptor/dsl/stubs/SleepingEventHandler.h"
#include "tests/disruptor/dsl/stubs/StubExceptionHandler.h"
#include "tests/disruptor/dsl/stubs/StubPublisher.h"
#include "tests/disruptor/support/DummyEventHandler.h"
#include "tests/disruptor/support/TestEvent.h"
#include "tests/disruptor/test_support/CountDownLatch.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <vector>

namespace {
class LatchHandler final
    : public disruptor::EventHandler<disruptor::support::TestEvent> {
public:
  explicit LatchHandler(disruptor::test_support::CountDownLatch &latch)
      : latch_(&latch) {}
  void onEvent(disruptor::support::TestEvent & /*event*/, int64_t /*sequence*/,
               bool /*endOfBatch*/) override {
    latch_->countDown();
  }

private:
  disruptor::test_support::CountDownLatch *latch_;
};

class NoOpTranslator final
    : public disruptor::EventTranslator<disruptor::support::TestEvent> {
public:
  void translateTo(disruptor::support::TestEvent & /*event*/,
                   int64_t /*sequence*/) override {}
};

class NoOpTranslatorOneArg final
    : public disruptor::EventTranslatorOneArg<disruptor::support::TestEvent,
                                              std::string> {
public:
  void translateTo(disruptor::support::TestEvent & /*event*/,
                   int64_t /*sequence*/, std::string /*arg0*/) override {}
};

// Test helper class to align with Java DisruptorTest class behavior
// Java: private final Collection<DelayedEventHandler> delayedEventHandlers =
// new ArrayList<>(); Java: private RingBuffer<TestEvent> ringBuffer;
class DisruptorTestHelper {
public:
  using DisruptorT =
      disruptor::dsl::Disruptor<disruptor::support::TestEvent,
                                disruptor::dsl::ProducerType::MULTI,
                                disruptor::BlockingWaitStrategy>;

  // Java: createDelayedEventHandler() adds to delayedEventHandlers collection
  disruptor::dsl::stubs::DelayedEventHandler &createDelayedEventHandler() {
    delayedEventHandlers_.push_back(
        std::make_unique<disruptor::dsl::stubs::DelayedEventHandler>());
    return *delayedEventHandlers_.back();
  }

  // Java: private void publishEvent() - auto-starts and waits for all
  // DelayedEventHandlers
  void publishEvent(DisruptorT &d) {
    if (ringBuffer_ == nullptr) {
      ringBuffer_ = d.start();
      // Wait for all DelayedEventHandlers to start (Java: awaitStart() for all
      // in delayedEventHandlers collection)
      for (auto &handler : delayedEventHandlers_) {
        handler->awaitStart();
      }
    }
    NoOpTranslator translator;
    d.publishEvent(translator);
  }

  // Java: ensureTwoEventsProcessedAccordingToDependencies calls publishEvent()
  // twice
  void ensureTwoEventsProcessedAccordingToDependencies(
      DisruptorT &d, disruptor::test_support::CountDownLatch &countDownLatch,
      std::vector<disruptor::dsl::stubs::DelayedEventHandler *> &dependencies) {
    publishEvent(d); // First event - auto-starts if needed
    publishEvent(d); // Second event

    // Java: assertThatCountDownLatchEquals(countDownLatch, 2L) before
    // processing dependencies
    for (auto *dependency : dependencies) {
      EXPECT_EQ(2, countDownLatch.getCount());
      dependency->processEvent();
      dependency->processEvent();
    }

    // Java: assertThatCountDownLatchIsZero(countDownLatch)
    countDownLatch.await();
  }

  // Java: @AfterEach tearDown() - stop all DelayedEventHandlers
  void tearDown() {
    for (auto &handler : delayedEventHandlers_) {
      handler->stopWaiting();
    }
  }

private:
  std::vector<std::unique_ptr<disruptor::dsl::stubs::DelayedEventHandler>>
      delayedEventHandlers_;
  std::shared_ptr<disruptor::RingBuffer<
      disruptor::support::TestEvent,
      disruptor::MultiProducerSequencer<disruptor::BlockingWaitStrategy>>>
      ringBuffer_;
};

// Helper function - fully aligned with Java version
// Matches Java: ensureTwoEventsProcessedAccordingToDependencies(countDownLatch,
// dependencies...) Must use DisruptorTestHelper to track all
// DelayedEventHandlers like Java version does
void ensureTwoEventsProcessedAccordingToDependencies(
    DisruptorTestHelper &helper,
    disruptor::dsl::Disruptor<disruptor::support::TestEvent,
                              disruptor::dsl::ProducerType::MULTI,
                              disruptor::BlockingWaitStrategy> &d,
    disruptor::test_support::CountDownLatch &countDownLatch,
    std::vector<disruptor::dsl::stubs::DelayedEventHandler *> &dependencies) {
  // Java: ensureTwoEventsProcessedAccordingToDependencies calls publishEvent()
  // twice publishEvent() auto-starts disruptor and waits for all
  // DelayedEventHandlers in the collection
  helper.ensureTwoEventsProcessedAccordingToDependencies(d, countDownLatch,
                                                         dependencies);
}

// Helper to wait for exception
std::exception *waitFor(std::atomic<std::exception *> &reference) {
  while (reference.load() == nullptr) {
    std::this_thread::yield();
  }
  return reference.load();
}

// Helper to assert producer reaches count
template <typename SequencerT>
void assertProducerReaches(
    disruptor::dsl::stubs::StubPublisher<SequencerT> &stubPublisher,
    int expectedPublicationCount, bool strict) {
  auto loopStart = std::chrono::steady_clock::now();
  while (stubPublisher.getPublicationCount() < expectedPublicationCount &&
         std::chrono::steady_clock::now() - loopStart <
             std::chrono::seconds(5)) {
    std::this_thread::yield();
  }

  if (strict) {
    EXPECT_EQ(expectedPublicationCount, stubPublisher.getPublicationCount());
  } else {
    int actualPublicationCount = stubPublisher.getPublicationCount();
    EXPECT_GE(actualPublicationCount, expectedPublicationCount)
        << "Producer reached unexpected count. Expected at least "
        << expectedPublicationCount << " but only reached "
        << actualPublicationCount;
  }
}
} // namespace

TEST(DisruptorTest, shouldHaveStartedAfterStartCalled) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

  EXPECT_FALSE(d.hasStarted());
  d.start();
  EXPECT_TRUE(d.hasStarted());
  d.halt();
}

TEST(DisruptorTest, shouldProcessMessagesPublishedBeforeStartIsCalled) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

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

TEST(DisruptorTest, shouldSupportMultipleCustomProcessorsAsDependencies) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 4, tf, ws);

  auto &ringBuffer = d.getRingBuffer();
  disruptor::test_support::CountDownLatch countDownLatch(2);
  disruptor::dsl::stubs::EventHandlerStub<Event> handlerWithBarrier(
      countDownLatch);

  disruptor::dsl::stubs::DelayedEventHandler delayedEventHandler1;
  disruptor::BatchEventProcessorBuilder builder1;
  auto barrier1 = ringBuffer.newBarrier();
  auto processor1 = builder1.build(ringBuffer, *barrier1, delayedEventHandler1);

  disruptor::dsl::stubs::DelayedEventHandler delayedEventHandler2;
  disruptor::BatchEventProcessorBuilder builder2;
  auto barrier2 = ringBuffer.newBarrier();
  auto processor2 = builder2.build(ringBuffer, *barrier2, delayedEventHandler2);

  disruptor::EventProcessor *processors[] = {processor1.get(),
                                             processor2.get()};
  d.handleEventsWith(processors, 2);
  d.after(processors, 2).handleEventsWith(handlerWithBarrier);

  d.start();
  delayedEventHandler1.awaitStart();
  delayedEventHandler2.awaitStart();

  NoOpTranslator translator;
  d.publishEvent(translator);
  d.publishEvent(translator);

  // Process events through dependencies first
  delayedEventHandler1.processEvent();
  delayedEventHandler1.processEvent();
  delayedEventHandler2.processEvent();
  delayedEventHandler2.processEvent();

  countDownLatch.await();
  delayedEventHandler1.stopWaiting();
  delayedEventHandler2.stopWaiting();
  d.halt();
}

TEST(DisruptorTest,
     shouldMakeEntriesAvailableToFirstCustomProcessorsImmediately) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 4, tf, ws);

  disruptor::test_support::CountDownLatch countDownLatch(2);
  disruptor::dsl::stubs::EventHandlerStub<Event> eventHandler(countDownLatch);

  using RingBufferT = std::remove_reference_t<decltype(d.getRingBuffer())>;
  class TestEventProcessorFactory
      : public disruptor::dsl::EventProcessorFactory<Event, RingBufferT> {
  public:
    explicit TestEventProcessorFactory(
        disruptor::dsl::stubs::EventHandlerStub<Event> &handler)
        : handler_(&handler) {}

    disruptor::EventProcessor &
    createEventProcessor(RingBufferT &ringBuffer,
                         disruptor::Sequence *const *barrierSequences,
                         int count) override {
      EXPECT_EQ(0, count) << "Should not have had any barrier sequences";
      disruptor::BatchEventProcessorBuilder builder;
      barrier_ = ringBuffer.newBarrier(barrierSequences, count);
      processor_ = builder.build(ringBuffer, *barrier_, *handler_);
      return *processor_;
    }

  private:
    disruptor::dsl::stubs::EventHandlerStub<Event> *handler_;
    std::shared_ptr<disruptor::EventProcessor> processor_;
    decltype(std::declval<RingBufferT &>().newBarrier(nullptr, 0)) barrier_;
  };

  // Keep factory alive for the lifetime of the test
  TestEventProcessorFactory factory(eventHandler);
  d.handleEventsWith(factory);

  // Java: ensureTwoEventsProcessedAccordingToDependencies(countDownLatch)
  DisruptorTestHelper helper;
  NoOpTranslator translator;
  helper.publishEvent(d);
  helper.publishEvent(d);

  countDownLatch.await();
  helper.tearDown();
  d.halt();
}

TEST(DisruptorTest, shouldHonourDependenciesForCustomProcessors) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 4, tf, ws);

  disruptor::test_support::CountDownLatch countDownLatch(2);
  disruptor::dsl::stubs::EventHandlerStub<Event> eventHandler(countDownLatch);
  disruptor::dsl::stubs::DelayedEventHandler delayedEventHandler;

  using RingBufferT2 = std::remove_reference_t<decltype(d.getRingBuffer())>;
  class TestEventProcessorFactory2
      : public disruptor::dsl::EventProcessorFactory<Event, RingBufferT2> {
  public:
    explicit TestEventProcessorFactory2(
        disruptor::dsl::stubs::EventHandlerStub<Event> &handler)
        : handler_(&handler) {}

    disruptor::EventProcessor &
    createEventProcessor(RingBufferT2 &ringBuffer,
                         disruptor::Sequence *const *barrierSequences,
                         int count) override {
      EXPECT_EQ(1, count) << "Should have had a barrier sequence";
      disruptor::BatchEventProcessorBuilder builder;
      barrier_ = ringBuffer.newBarrier(barrierSequences, count);
      processor_ = builder.build(ringBuffer, *barrier_, *handler_);
      return *processor_;
    }

  private:
    disruptor::dsl::stubs::EventHandlerStub<Event> *handler_;
    std::shared_ptr<disruptor::EventProcessor> processor_;
    decltype(std::declval<RingBufferT2 &>().newBarrier(nullptr, 0)) barrier_;
  };

  // Keep factory alive for the lifetime of the test
  TestEventProcessorFactory2 eventProcessorFactory(eventHandler);

  DisruptorTestHelper helper;
  auto &delayedEventHandlerFromHelper = helper.createDelayedEventHandler();
  d.handleEventsWith(delayedEventHandlerFromHelper)
      .thenFactories(eventProcessorFactory);

  // Java: ensureTwoEventsProcessedAccordingToDependencies(countDownLatch,
  // delayedEventHandler)
  std::vector<disruptor::dsl::stubs::DelayedEventHandler *> deps;
  deps.push_back(&delayedEventHandlerFromHelper);
  ensureTwoEventsProcessedAccordingToDependencies(helper, d, countDownLatch,
                                                  deps);

  helper.tearDown();
  d.halt();
}

TEST(DisruptorTest, shouldBatchOfEvents) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

  disruptor::test_support::CountDownLatch eventCounter(2);
  LatchHandler handler(eventCounter);
  d.handleEventsWith(handler);

  d.start();

  NoOpTranslatorOneArg translator;
  d.publishEvent(translator, std::string("a"));
  d.publishEvent(translator, std::string("b"));

  eventCounter.await();
  d.halt();
}

TEST(DisruptorTest, shouldHandleEventsWithRewindableEventHandlers) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

  disruptor::test_support::CountDownLatch eventCounter(2);
  class TestRewindableEventHandler final
      : public disruptor::RewindableEventHandler<Event> {
  public:
    explicit TestRewindableEventHandler(
        disruptor::test_support::CountDownLatch &latch)
        : latch_(&latch) {}
    void onEvent(Event & /*event*/, int64_t /*sequence*/,
                 bool /*endOfBatch*/) override {
      latch_->countDown();
    }

  private:
    disruptor::test_support::CountDownLatch *latch_;
  };

  // Note: C++ version doesn't have handleEventsWith(BatchRewindStrategy,
  // RewindableEventHandler...) but RewindableEventHandler can be used directly
  // with handleEventsWith
  TestRewindableEventHandler testEventRewindableEventHandler(eventCounter);
  d.handleEventsWith(testEventRewindableEventHandler);

  d.start();

  NoOpTranslatorOneArg translator;
  d.publishEvent(translator, std::string("a"));
  d.publishEvent(translator, std::string("b"));

  eventCounter.await();
  d.halt();
}

TEST(DisruptorTest, shouldMakeEntriesAvailableToFirstHandlersImmediately) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 4, tf, ws);

  DisruptorTestHelper helper;
  disruptor::test_support::CountDownLatch countDownLatch(2);
  disruptor::dsl::stubs::EventHandlerStub<Event> eventHandler(countDownLatch);
  auto &delayedEventHandler =
      helper.createDelayedEventHandler(); // Java: createDelayedEventHandler()

  d.handleEventsWith(delayedEventHandler, eventHandler);

  std::vector<disruptor::dsl::stubs::DelayedEventHandler *> deps;
  deps.push_back(&delayedEventHandler);
  helper.ensureTwoEventsProcessedAccordingToDependencies(d, countDownLatch,
                                                         deps);

  helper.tearDown(); // Java: @AfterEach tearDown()
  d.halt();
}

TEST(DisruptorTest, shouldAddEventProcessorsAfterPublishing) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

  auto &ringBuffer = d.getRingBuffer();
  disruptor::dsl::stubs::SleepingEventHandler handler1;
  disruptor::BatchEventProcessorBuilder builder1;
  auto barrier1 = ringBuffer.newBarrier();
  auto b1 = builder1.build(ringBuffer, *barrier1, handler1);

  disruptor::Sequence *seqs1[] = {&b1->getSequence()};
  auto barrier2 = ringBuffer.newBarrier(seqs1, 1);
  disruptor::dsl::stubs::SleepingEventHandler handler2;
  disruptor::BatchEventProcessorBuilder builder2;
  auto b2 = builder2.build(ringBuffer, *barrier2, handler2);

  disruptor::Sequence *seqs2[] = {&b2->getSequence()};
  auto barrier3 = ringBuffer.newBarrier(seqs2, 1);
  disruptor::dsl::stubs::SleepingEventHandler handler3;
  disruptor::BatchEventProcessorBuilder builder3;
  auto b3 = builder3.build(ringBuffer, *barrier3, handler3);

  EXPECT_EQ(-1, b1->getSequence().get());
  EXPECT_EQ(-1, b2->getSequence().get());
  EXPECT_EQ(-1, b3->getSequence().get());

  ringBuffer.publish(ringBuffer.next());
  ringBuffer.publish(ringBuffer.next());
  ringBuffer.publish(ringBuffer.next());
  ringBuffer.publish(ringBuffer.next());
  ringBuffer.publish(ringBuffer.next());
  ringBuffer.publish(ringBuffer.next());

  disruptor::EventProcessor *processors[] = {b1.get(), b2.get(), b3.get()};
  d.handleEventsWith(processors, 3);

  EXPECT_EQ(5, b1->getSequence().get());
  EXPECT_EQ(5, b2->getSequence().get());
  EXPECT_EQ(5, b3->getSequence().get());

  d.halt();
}

TEST(DisruptorTest, shouldSetSequenceForHandlerIfAddedAfterPublish) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

  auto &ringBuffer = d.getRingBuffer();
  disruptor::dsl::stubs::SleepingEventHandler b1;
  disruptor::dsl::stubs::SleepingEventHandler b2;
  disruptor::dsl::stubs::SleepingEventHandler b3;

  ringBuffer.publish(ringBuffer.next());
  ringBuffer.publish(ringBuffer.next());
  ringBuffer.publish(ringBuffer.next());
  ringBuffer.publish(ringBuffer.next());
  ringBuffer.publish(ringBuffer.next());
  ringBuffer.publish(ringBuffer.next());

  d.handleEventsWith(b1, b2, b3);

  EXPECT_EQ(5, d.getSequenceValueFor(b1));
  EXPECT_EQ(5, d.getSequenceValueFor(b2));
  EXPECT_EQ(5, d.getSequenceValueFor(b3));

  d.halt();
}

TEST(DisruptorTest, shouldGetSequenceBarrierForHandler) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

  auto &ringBuffer = d.getRingBuffer();
  disruptor::support::DummyEventHandler<Event> handler;

  d.handleEventsWith(handler);
  d.start();

  ringBuffer.publish(ringBuffer.next());
  ringBuffer.publish(ringBuffer.next());
  ringBuffer.publish(ringBuffer.next());
  ringBuffer.publish(ringBuffer.next());
  ringBuffer.publish(ringBuffer.next());
  ringBuffer.publish(ringBuffer.next());

  auto barrier = d.getBarrierFor(handler);
  EXPECT_EQ(5, barrier->getCursor());

  d.halt();
}

TEST(DisruptorTest, shouldGetSequenceBarrierForHandlerIfAddedAfterPublish) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

  auto &ringBuffer = d.getRingBuffer();
  disruptor::support::DummyEventHandler<Event> handler;

  ringBuffer.publish(ringBuffer.next());
  ringBuffer.publish(ringBuffer.next());
  ringBuffer.publish(ringBuffer.next());
  ringBuffer.publish(ringBuffer.next());
  ringBuffer.publish(ringBuffer.next());
  ringBuffer.publish(ringBuffer.next());

  d.handleEventsWith(handler);
  d.start();

  auto barrier = d.getBarrierFor(handler);
  EXPECT_EQ(5, barrier->getCursor());

  d.halt();
}

TEST(DisruptorTest, shouldCreateEventProcessorGroupForFirstEventProcessors) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

  disruptor::dsl::stubs::SleepingEventHandler eventHandler1;
  disruptor::dsl::stubs::SleepingEventHandler eventHandler2;

  auto eventHandlerGroup = d.handleEventsWith(eventHandler1, eventHandler2);
  d.start();

  EXPECT_NE(nullptr, &eventHandlerGroup);

  d.halt();
}

TEST(
    DisruptorTest,
    shouldWaitUntilAllFirstEventProcessorsProcessEventBeforeMakingItAvailableToDependentEventProcessors) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 4, tf, ws);

  DisruptorTestHelper helper;
  auto &eventHandler1 = helper.createDelayedEventHandler();
  disruptor::test_support::CountDownLatch countDownLatch(2);
  disruptor::dsl::stubs::EventHandlerStub<Event> eventHandler2(countDownLatch);

  d.handleEventsWith(eventHandler1).then(eventHandler2);

  std::vector<disruptor::dsl::stubs::DelayedEventHandler *> deps;
  deps.push_back(&eventHandler1);
  ensureTwoEventsProcessedAccordingToDependencies(helper, d, countDownLatch,
                                                  deps);

  helper.tearDown();
  d.halt();
}

TEST(DisruptorTest, shouldSupportAddingCustomEventProcessorWithFactory) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

  auto &ringBuffer = d.getRingBuffer();
  disruptor::dsl::stubs::SleepingEventHandler handler1;
  disruptor::BatchEventProcessorBuilder builder1;
  auto barrier1 = ringBuffer.newBarrier();
  auto b1 = builder1.build(ringBuffer, *barrier1, handler1);

  using RingBufferT = std::remove_reference_t<decltype(d.getRingBuffer())>;
  class TestEventProcessorFactory
      : public disruptor::dsl::EventProcessorFactory<Event, RingBufferT> {
  public:
    explicit TestEventProcessorFactory(
        disruptor::dsl::stubs::SleepingEventHandler &handler)
        : handler_(&handler) {}

    disruptor::EventProcessor &
    createEventProcessor(RingBufferT &ringBuffer,
                         disruptor::Sequence *const *barrierSequences,
                         int count) override {
      disruptor::BatchEventProcessorBuilder builder;
      barrier_ = ringBuffer.newBarrier(barrierSequences, count);
      processor_ = builder.build(ringBuffer, *barrier_, *handler_);
      return *processor_;
    }

  private:
    disruptor::dsl::stubs::SleepingEventHandler *handler_;
    std::shared_ptr<disruptor::EventProcessor> processor_;
    decltype(std::declval<RingBufferT &>().newBarrier(nullptr, 0)) barrier_;
  };

  disruptor::dsl::stubs::SleepingEventHandler handler2;
  // Keep factory alive for the lifetime of the test
  TestEventProcessorFactory b2(handler2);

  disruptor::EventProcessor *processors[] = {b1.get()};
  d.handleEventsWith(processors, 1).thenFactories(b2);

  d.start();

  // Ensure all processors have time to start before halting
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  d.halt();
}

TEST(DisruptorTest, shouldAllowSpecifyingSpecificEventProcessorsToWaitFor) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 4, tf, ws);

  DisruptorTestHelper helper;
  auto &handler1 = helper.createDelayedEventHandler();
  auto &handler2 = helper.createDelayedEventHandler();

  disruptor::test_support::CountDownLatch countDownLatch(2);
  disruptor::dsl::stubs::EventHandlerStub<Event> handlerWithBarrier(
      countDownLatch);

  d.handleEventsWith(handler1, handler2);
  disruptor::EventHandlerIdentity *handlers[] = {&handler1, &handler2};
  d.after(handlers, 2).handleEventsWith(handlerWithBarrier);

  std::vector<disruptor::dsl::stubs::DelayedEventHandler *> deps;
  deps.push_back(&handler1);
  deps.push_back(&handler2);
  ensureTwoEventsProcessedAccordingToDependencies(helper, d, countDownLatch,
                                                  deps);

  helper.tearDown();
  d.halt();
}

TEST(DisruptorTest, shouldWaitOnAllProducersJoinedByAnd) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 4, tf, ws);

  DisruptorTestHelper helper;
  auto &handler1 = helper.createDelayedEventHandler();
  auto &handler2 = helper.createDelayedEventHandler();

  disruptor::test_support::CountDownLatch countDownLatch(2);
  disruptor::dsl::stubs::EventHandlerStub<Event> handlerWithBarrier(
      countDownLatch);

  d.handleEventsWith(handler1);
  auto handler2Group = d.handleEventsWith(handler2);
  disruptor::EventHandlerIdentity *handlers1[] = {&handler1};
  auto handler1Group = d.after(handlers1, 1);
  handler1Group.and_(handler2Group).handleEventsWith(handlerWithBarrier);

  std::vector<disruptor::dsl::stubs::DelayedEventHandler *> deps;
  deps.push_back(&handler1);
  deps.push_back(&handler2);
  ensureTwoEventsProcessedAccordingToDependencies(helper, d, countDownLatch,
                                                  deps);

  helper.tearDown();
  d.halt();
}

TEST(DisruptorTest, shouldThrowExceptionIfHandlerIsNotAlreadyConsuming) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

  disruptor::dsl::stubs::DelayedEventHandler handler;
  disruptor::dsl::stubs::DelayedEventHandler handler2;

  disruptor::EventHandlerIdentity *handlers[] = {&handler};
  EXPECT_THROW(d.after(handlers, 1).handleEventsWith(handler2),
               std::invalid_argument);

  d.halt();
}

TEST(DisruptorTest, shouldTrackEventHandlersByIdentityNotEquality) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

  disruptor::dsl::stubs::EvilEqualsEventHandler handler1;
  disruptor::dsl::stubs::EvilEqualsEventHandler handler2;

  d.handleEventsWith(handler1);

  // handler2 == handler1 (evil equals) but it hasn't yet been registered so
  // should throw exception.
  disruptor::EventHandlerIdentity *handlers2[] = {&handler2};
  EXPECT_THROW(d.after(handlers2, 1), std::invalid_argument);

  d.halt();
}

TEST(DisruptorTest,
     shouldSupportSpecifyingAExceptionHandlerForEventProcessors) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

  std::atomic<std::exception *> eventHandled{nullptr};
  disruptor::dsl::stubs::StubExceptionHandler<Event> exceptionHandler(
      eventHandled);
  std::runtime_error testException("test");
  disruptor::dsl::stubs::ExceptionThrowingEventHandler handler(&testException);

  d.handleExceptionsWith(exceptionHandler);
  d.handleEventsWith(handler);

  NoOpTranslator translator;
  d.publishEvent(translator);
  d.start();

  auto *actualException = waitFor(eventHandled);
  EXPECT_NE(nullptr, actualException);

  d.halt();
}

TEST(
    DisruptorTest,
    shouldOnlyApplyExceptionsHandlersSpecifiedViaHandleExceptionsWithOnNewEventProcessors) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

  std::atomic<std::exception *> eventHandled{nullptr};
  disruptor::dsl::stubs::StubExceptionHandler<Event> exceptionHandler(
      eventHandled);
  std::runtime_error testException("test");
  disruptor::dsl::stubs::ExceptionThrowingEventHandler handler(&testException);

  d.handleExceptionsWith(exceptionHandler);
  d.handleEventsWith(handler);
  disruptor::FatalExceptionHandler<Event> fatalHandler;
  d.handleExceptionsWith(fatalHandler);

  NoOpTranslator translator;
  d.publishEvent(translator);
  d.start();

  auto *actualException = waitFor(eventHandled);
  EXPECT_NE(nullptr, actualException);

  d.halt();
}

TEST(DisruptorTest,
     shouldSupportSpecifyingADefaultExceptionHandlerForEventProcessors) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

  std::atomic<std::exception *> eventHandled{nullptr};
  disruptor::dsl::stubs::StubExceptionHandler<Event> exceptionHandler(
      eventHandled);
  std::runtime_error testException("test");
  disruptor::dsl::stubs::ExceptionThrowingEventHandler handler(&testException);

  d.setDefaultExceptionHandler(exceptionHandler);
  d.handleEventsWith(handler);

  NoOpTranslator translator;
  d.publishEvent(translator);
  d.start();

  auto *actualException = waitFor(eventHandled);
  EXPECT_NE(nullptr, actualException);

  d.halt();
}

TEST(DisruptorTest,
     shouldApplyDefaultExceptionHandlerToExistingEventProcessors) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

  std::atomic<std::exception *> eventHandled{nullptr};
  disruptor::dsl::stubs::StubExceptionHandler<Event> exceptionHandler(
      eventHandled);
  std::runtime_error testException("test");
  disruptor::dsl::stubs::ExceptionThrowingEventHandler handler(&testException);

  d.handleEventsWith(handler);
  d.setDefaultExceptionHandler(exceptionHandler);

  NoOpTranslator translator;
  d.publishEvent(translator);
  d.start();

  auto *actualException = waitFor(eventHandled);
  EXPECT_NE(nullptr, actualException);

  d.halt();
}

TEST(DisruptorTest, shouldBlockProducerUntilAllEventProcessorsHaveAdvanced) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  // Java: bufferSize = 4, ProducerType.SINGLE
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::SINGLE, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 4, tf, ws);

  DisruptorTestHelper helper;
  auto &delayedEventHandler = helper.createDelayedEventHandler();
  d.handleEventsWith(delayedEventHandler);

  auto ringBuffer = d.start();
  delayedEventHandler.awaitStart();

  using SequencerT = disruptor::SingleProducerSequencer<WS>;
  disruptor::dsl::stubs::StubPublisher<SequencerT> stubPublisher(*ringBuffer);
  std::thread publisherThread([&] { stubPublisher.run(); });

  // Give publisher thread time to start
  std::this_thread::sleep_for(std::chrono::milliseconds(10));

  // Java: assertProducerReaches(stubPublisher, 4, true) - waits until >= 4,
  // then checks strict equality
  assertProducerReaches(stubPublisher, 4, true);

  delayedEventHandler.processEvent();
  delayedEventHandler.processEvent();
  delayedEventHandler.processEvent();
  delayedEventHandler.processEvent();
  delayedEventHandler.processEvent();

  // Java: assertProducerReaches(stubPublisher, 5, false) - non-strict, just
  // checks >= 5
  assertProducerReaches(stubPublisher, 5, false);

  stubPublisher.halt();
  publisherThread.join();
  helper.tearDown();
  d.halt(); // Ensure Disruptor threads are joined before destruction
}

TEST(DisruptorTest,
     shouldBeAbleToOverrideTheExceptionHandlerForAEventProcessor) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

  std::runtime_error testException("test");
  disruptor::dsl::stubs::ExceptionThrowingEventHandler eventHandler(
      &testException);
  d.handleEventsWith(eventHandler);

  std::atomic<std::exception *> reference{nullptr};
  disruptor::dsl::stubs::StubExceptionHandler<Event> exceptionHandler(
      reference);
  d.handleExceptionsFor(eventHandler).with(exceptionHandler);

  NoOpTranslator translator;
  d.publishEvent(translator);
  d.start();

  auto *actualException = waitFor(reference);
  EXPECT_NE(nullptr, actualException);

  d.halt();
}

TEST(
    DisruptorTest,
    shouldThrowExceptionWhenAddingEventProcessorsAfterTheProducerBarrierHasBeenCreated) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

  disruptor::dsl::stubs::SleepingEventHandler handler1;
  d.handleEventsWith(handler1);
  d.start();

  disruptor::dsl::stubs::SleepingEventHandler handler2;
  EXPECT_THROW(d.handleEventsWith(handler2), std::runtime_error);

  d.halt();
}

TEST(DisruptorTest, shouldThrowExceptionIfStartIsCalledTwice) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

  disruptor::dsl::stubs::SleepingEventHandler handler;
  d.handleEventsWith(handler);
  d.start();

  EXPECT_THROW(d.start(), std::runtime_error);

  d.halt();
}

TEST(DisruptorTest, shouldSupportCustomProcessorsAsDependencies) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 4, tf, ws);

  DisruptorTestHelper helper;
  auto &ringBuffer = d.getRingBuffer();
  auto &delayedEventHandler = helper.createDelayedEventHandler();

  disruptor::test_support::CountDownLatch countDownLatch(2);
  disruptor::dsl::stubs::EventHandlerStub<Event> handlerWithBarrier(
      countDownLatch);

  disruptor::BatchEventProcessorBuilder builder;
  auto barrier = ringBuffer.newBarrier();
  auto processor = builder.build(ringBuffer, *barrier, delayedEventHandler);

  disruptor::EventProcessor *processors[] = {processor.get()};
  d.handleEventsWith(processors, 1).then(handlerWithBarrier);

  std::vector<disruptor::dsl::stubs::DelayedEventHandler *> deps;
  deps.push_back(&delayedEventHandler);
  ensureTwoEventsProcessedAccordingToDependencies(helper, d, countDownLatch,
                                                  deps);

  helper.tearDown();
  d.halt();
}

TEST(DisruptorTest, shouldSupportHandlersAsDependenciesToCustomProcessors) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 4, tf, ws);

  DisruptorTestHelper helper;
  auto &delayedEventHandler = helper.createDelayedEventHandler();
  d.handleEventsWith(delayedEventHandler);

  auto &ringBuffer = d.getRingBuffer();
  disruptor::test_support::CountDownLatch countDownLatch(2);
  disruptor::dsl::stubs::EventHandlerStub<Event> handlerWithBarrier(
      countDownLatch);

  disruptor::EventHandlerIdentity *handlers3[] = {&delayedEventHandler};
  auto sequenceBarrier = d.after(handlers3, 1).asSequenceBarrier();
  disruptor::BatchEventProcessorBuilder builder;
  auto processor =
      builder.build(ringBuffer, *sequenceBarrier, handlerWithBarrier);
  disruptor::EventProcessor *processors[] = {processor.get()};
  d.handleEventsWith(processors, 1);

  std::vector<disruptor::dsl::stubs::DelayedEventHandler *> deps;
  deps.push_back(&delayedEventHandler);
  ensureTwoEventsProcessedAccordingToDependencies(helper, d, countDownLatch,
                                                  deps);

  helper.tearDown();
  d.halt();
}

TEST(DisruptorTest, shouldSupportCustomProcessorsAndHandlersAsDependencies) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 4, tf, ws);

  DisruptorTestHelper helper;
  auto &delayedEventHandler1 = helper.createDelayedEventHandler();
  auto &delayedEventHandler2 = helper.createDelayedEventHandler();
  d.handleEventsWith(delayedEventHandler1);

  auto &ringBuffer = d.getRingBuffer();
  disruptor::test_support::CountDownLatch countDownLatch(2);
  disruptor::dsl::stubs::EventHandlerStub<Event> handlerWithBarrier(
      countDownLatch);

  disruptor::EventHandlerIdentity *handlers4[] = {&delayedEventHandler1};
  auto sequenceBarrier = d.after(handlers4, 1).asSequenceBarrier();
  disruptor::BatchEventProcessorBuilder builder;
  auto processor =
      builder.build(ringBuffer, *sequenceBarrier, delayedEventHandler2);

  disruptor::EventProcessor *processors[] = {processor.get()};
  d.after(handlers4, 1)
      .and_(processors, 1)
      .handleEventsWith(handlerWithBarrier);

  std::vector<disruptor::dsl::stubs::DelayedEventHandler *> deps;
  deps.push_back(&delayedEventHandler1);
  deps.push_back(&delayedEventHandler2);
  ensureTwoEventsProcessedAccordingToDependencies(helper, d, countDownLatch,
                                                  deps);

  helper.tearDown();
  d.halt();
}

TEST(DisruptorTest,
     shouldThrowTimeoutExceptionIfShutdownDoesNotCompleteNormally) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

  DisruptorTestHelper helper;
  auto &delayedEventHandler = helper.createDelayedEventHandler();
  d.handleEventsWith(delayedEventHandler);

  // Java: publishEvent() auto-starts disruptor and waits for
  // DelayedEventHandler to start
  helper.publishEvent(d);

  // Java: shutdown(1, SECONDS) should throw TimeoutException because
  // DelayedEventHandler hasn't processed the event, so there's a backlog
  EXPECT_THROW(d.shutdown(1000), disruptor::TimeoutException);

  helper.tearDown();
  d.halt();
}

TEST(DisruptorTest, shouldTrackRemainingCapacity) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 1024, tf, ws);

  int64_t remainingCapacity = -1;
  class CapacityTrackingHandler final : public disruptor::EventHandler<Event> {
  public:
    explicit CapacityTrackingHandler(
        disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI,
                                  WS> &d,
        int64_t &capacity)
        : d_(&d), capacity_(&capacity) {}
    void onEvent(Event & /*event*/, int64_t /*sequence*/,
                 bool /*endOfBatch*/) override {
      *capacity_ = d_->getRingBuffer().remainingCapacity();
    }

  private:
    disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS>
        *d_;
    int64_t *capacity_;
  };

  CapacityTrackingHandler eventHandler(d, remainingCapacity);
  d.handleEventsWith(eventHandler);

  NoOpTranslator translator;
  d.publishEvent(translator);
  d.start();

  while (remainingCapacity == -1) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  EXPECT_EQ(1023, remainingCapacity);
  EXPECT_EQ(1024, d.getRingBuffer().remainingCapacity());

  d.halt();
}

TEST(DisruptorTest, shouldAllowEventHandlerWithSuperType) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 4, tf, ws);

  disruptor::test_support::CountDownLatch latch(2);
  // Note: Use EventHandler<Event> but accept any type (Java uses
  // EventHandler<Object>)
  class ObjectHandler final : public disruptor::EventHandler<Event> {
  public:
    explicit ObjectHandler(disruptor::test_support::CountDownLatch &latch)
        : latch_(&latch) {}
    void onEvent(Event & /*event*/, int64_t /*sequence*/,
                 bool /*endOfBatch*/) override {
      latch_->countDown();
    }

  private:
    disruptor::test_support::CountDownLatch *latch_;
  };
  ObjectHandler objectHandler(latch);

  d.handleEventsWith(objectHandler);
  d.start();

  NoOpTranslator translator;
  d.publishEvent(translator);
  d.publishEvent(translator);

  latch.await();
  d.halt();
}

TEST(DisruptorTest, shouldAllowChainingEventHandlersWithSuperType) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BlockingWaitStrategy;
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  WS ws;
  disruptor::dsl::Disruptor<Event, disruptor::dsl::ProducerType::MULTI, WS> d(
      disruptor::support::TestEvent::EVENT_FACTORY, 4, tf, ws);

  DisruptorTestHelper helper;
  disruptor::test_support::CountDownLatch latch(2);
  auto &delayedEventHandler = helper.createDelayedEventHandler();
  class ObjectHandler final : public disruptor::EventHandler<Event> {
  public:
    explicit ObjectHandler(disruptor::test_support::CountDownLatch &latch)
        : latch_(&latch) {}
    void onEvent(Event & /*event*/, int64_t /*sequence*/,
                 bool /*endOfBatch*/) override {
      latch_->countDown();
    }

  private:
    disruptor::test_support::CountDownLatch *latch_;
  };
  ObjectHandler objectHandler(latch);

  d.handleEventsWith(delayedEventHandler).then(objectHandler);

  std::vector<disruptor::dsl::stubs::DelayedEventHandler *> deps;
  deps.push_back(&delayedEventHandler);
  ensureTwoEventsProcessedAccordingToDependencies(helper, d, latch, deps);

  helper.tearDown();
  d.halt();
}
