#include <gtest/gtest.h>

#include "disruptor/AlertException.h"
#include "disruptor/NoOpEventProcessor.h"
#include "disruptor/RingBuffer.h"
#include "disruptor/Sequence.h"
#include "disruptor/SequenceBarrier.h"
#include "disruptor/util/Util.h"
#include "tests/disruptor/support/StubEvent.h"
#include "tests/disruptor/support/DummyEventProcessor.h"
#include "tests/disruptor/test_support/CountDownLatch.h"

#include <memory>
#include <thread>

namespace {
void fillRingBuffer(disruptor::RingBuffer<disruptor::support::StubEvent>& ringBuffer, int64_t expectedNumberMessages) {
  for (int64_t i = 0; i < expectedNumberMessages; ++i) {
    const int64_t sequence = ringBuffer.next();
    auto& event = ringBuffer.get(sequence);
    event.setValue(static_cast<int>(i));
    ringBuffer.publish(sequence);
  }
}

class CountDownLatchSequence final : public disruptor::Sequence {
public:
  CountDownLatchSequence(int64_t initialValue, disruptor::test_support::CountDownLatch& latch)
      : disruptor::Sequence(initialValue), latch_(&latch) {}

  int64_t get() const noexcept override {
    latch_->countDown();
    return disruptor::Sequence::get();
  }

private:
  disruptor::test_support::CountDownLatch* latch_;
};
} // namespace

class SequenceBarrierTestFixture : public ::testing::Test {
protected:
  std::shared_ptr<disruptor::RingBuffer<disruptor::support::StubEvent>> ringBuffer =
      disruptor::RingBuffer<disruptor::support::StubEvent>::createMultiProducer(disruptor::support::StubEvent::EVENT_FACTORY, 64);

  SequenceBarrierTestFixture() {
    ringBuffer->addGatingSequences(noOp.getSequence());
  }

  disruptor::NoOpEventProcessor<disruptor::support::StubEvent> noOp{*ringBuffer};
};

TEST_F(SequenceBarrierTestFixture, shouldWaitForWorkCompleteWhereCompleteWorkThresholdIsAhead) {
  const int64_t expectedNumberMessages = 10;
  const int64_t expectedWorkSequence = 9;
  fillRingBuffer(*ringBuffer, expectedNumberMessages);

  disruptor::Sequence sequence1(expectedNumberMessages);
  disruptor::Sequence sequence2(expectedWorkSequence);
  disruptor::Sequence sequence3(expectedNumberMessages);

  disruptor::Sequence* deps[] = {&sequence1, &sequence2, &sequence3};
  auto sequenceBarrier = ringBuffer->newBarrier(deps, 3);

  const int64_t completedWorkSequence = sequenceBarrier->waitFor(expectedWorkSequence);
  EXPECT_GE(completedWorkSequence, expectedWorkSequence);
}

TEST_F(SequenceBarrierTestFixture, shouldWaitForWorkCompleteWhereAllWorkersAreBlockedOnRingBuffer) {
  const int64_t expectedNumberMessages = 10;
  fillRingBuffer(*ringBuffer, expectedNumberMessages);

  disruptor::support::DummyEventProcessor workers[3];
  for (auto& w : workers) {
    w.setSequence(expectedNumberMessages - 1);
  }

  std::vector<disruptor::Sequence*> deps;
  deps.reserve(3);
  for (auto& w : workers) {
    deps.push_back(&w.getSequence());
  }

  auto sequenceBarrier = ringBuffer->newBarrier(deps.data(), static_cast<int>(deps.size()));

  std::thread t([&] {
    const int64_t sequence = ringBuffer->next();
    auto& event = ringBuffer->get(sequence);
    event.setValue(static_cast<int>(sequence));
    ringBuffer->publish(sequence);

    for (auto& w : workers) {
      w.setSequence(sequence);
    }
  });

  const int64_t expectedWorkSequence = expectedNumberMessages;
  const int64_t completedWorkSequence = sequenceBarrier->waitFor(expectedNumberMessages);
  EXPECT_GE(completedWorkSequence, expectedWorkSequence);

  t.join();
}

TEST_F(SequenceBarrierTestFixture, shouldInterruptDuringBusySpin) {
  const int64_t expectedNumberMessages = 10;
  fillRingBuffer(*ringBuffer, expectedNumberMessages);

  disruptor::test_support::CountDownLatch latch(3);
  CountDownLatchSequence sequence1(8, latch);
  CountDownLatchSequence sequence2(8, latch);
  CountDownLatchSequence sequence3(8, latch);

  disruptor::Sequence* deps[] = {&sequence1, &sequence2, &sequence3};
  auto sequenceBarrier = ringBuffer->newBarrier(deps, 3);

  std::thread t([&] {
    EXPECT_THROW((void)sequenceBarrier->waitFor(expectedNumberMessages - 1), disruptor::AlertException);
  });

  latch.await();
  sequenceBarrier->alert();
  t.join();
}

TEST_F(SequenceBarrierTestFixture, shouldWaitForWorkCompleteWhereCompleteWorkThresholdIsBehind) {
  const int64_t expectedNumberMessages = 10;
  fillRingBuffer(*ringBuffer, expectedNumberMessages);

  disruptor::support::DummyEventProcessor eventProcessors[3];
  for (auto& p : eventProcessors) {
    p.setSequence(expectedNumberMessages - 2);
  }

  std::vector<disruptor::Sequence*> deps;
  deps.reserve(3);
  for (auto& p : eventProcessors) {
    deps.push_back(&p.getSequence());
  }

  auto sequenceBarrier = ringBuffer->newBarrier(deps.data(), static_cast<int>(deps.size()));

  std::thread t([&] {
    for (auto& p : eventProcessors) {
      p.setSequence(p.getSequence().get() + 1);
    }
  });
  t.join();

  const int64_t expectedWorkSequence = expectedNumberMessages - 1;
  const int64_t completedWorkSequence = sequenceBarrier->waitFor(expectedWorkSequence);
  EXPECT_GE(completedWorkSequence, expectedWorkSequence);
}

TEST_F(SequenceBarrierTestFixture, shouldSetAndClearAlertStatus) {
  auto sequenceBarrier = ringBuffer->newBarrier(nullptr, 0);

  EXPECT_FALSE(sequenceBarrier->isAlerted());
  sequenceBarrier->alert();
  EXPECT_TRUE(sequenceBarrier->isAlerted());
  sequenceBarrier->clearAlert();
  EXPECT_FALSE(sequenceBarrier->isAlerted());
}
