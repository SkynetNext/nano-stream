#include <gtest/gtest.h>

#include "disruptor/InsufficientCapacityException.h"
#include "disruptor/RingBuffer.h"
#include "disruptor/Sequencer.h"
#include "tests/disruptor/support/StubEvent.h"

#include <random>

namespace {
class AssertingSequencer final : public disruptor::Sequencer {
public:
  explicit AssertingSequencer(int size) : size_(size) {}

  int getBufferSize() const override { return size_; }
  bool hasAvailableCapacity(int requiredCapacity) override { return requiredCapacity <= size_; }
  int64_t remainingCapacity() override { return size_; }

  int64_t next() override {
    lastValue_ = dist_(rng_);
    lastBatchSize_ = 1;
    return lastValue_;
  }
  int64_t next(int n) override {
    lastValue_ = std::max<int64_t>(n, dist_(rng_));
    lastBatchSize_ = n;
    return lastValue_;
  }

  int64_t tryNext() override { return next(); }
  int64_t tryNext(int n) override { return next(n); }

  void publish(int64_t sequence) override {
    EXPECT_EQ(sequence, lastValue_);
    EXPECT_EQ(lastBatchSize_, 1);
  }
  void publish(int64_t lo, int64_t hi) override {
    EXPECT_EQ(hi, lastValue_);
    EXPECT_EQ((hi - lo) + 1, lastBatchSize_);
  }

  int64_t getCursor() const override { return lastValue_; }
  void claim(int64_t /*sequence*/) override {}
  bool isAvailable(int64_t /*sequence*/) override { return false; }
  void addGatingSequences(disruptor::Sequence* const* /*gatingSequences*/, int /*count*/) override {}
  bool removeGatingSequence(disruptor::Sequence& /*sequence*/) override { return false; }
  std::shared_ptr<disruptor::SequenceBarrier> newBarrier(disruptor::Sequence* const* /*sequencesToTrack*/, int /*count*/) override {
    return nullptr;
  }
  int64_t getMinimumSequence() override { return 0; }
  int64_t getHighestPublishedSequence(int64_t /*nextSequence*/, int64_t /*availableSequence*/) override { return 0; }

private:
  int size_;
  int64_t lastBatchSize_{-1};
  int64_t lastValue_{-1};
  std::mt19937_64 rng_{123};
  std::uniform_int_distribution<int64_t> dist_{0, 1'000'000};
};
} // namespace

TEST(RingBufferWithAssertingStubTest, shouldDelegateNextAndPublish) {
  auto sequencer = std::make_shared<AssertingSequencer>(16);
  auto ringBuffer = std::make_shared<disruptor::RingBuffer<disruptor::support::StubEvent>>(disruptor::support::StubEvent::EVENT_FACTORY, sequencer);
  ringBuffer->publish(ringBuffer->next());
}

TEST(RingBufferWithAssertingStubTest, shouldDelegateTryNextAndPublish) {
  auto sequencer = std::make_shared<AssertingSequencer>(16);
  auto ringBuffer = std::make_shared<disruptor::RingBuffer<disruptor::support::StubEvent>>(disruptor::support::StubEvent::EVENT_FACTORY, sequencer);
  ringBuffer->publish(ringBuffer->tryNext());
}

TEST(RingBufferWithAssertingStubTest, shouldDelegateNextNAndPublish) {
  auto sequencer = std::make_shared<AssertingSequencer>(16);
  auto ringBuffer = std::make_shared<disruptor::RingBuffer<disruptor::support::StubEvent>>(disruptor::support::StubEvent::EVENT_FACTORY, sequencer);
  int64_t hi = ringBuffer->next(10);
  ringBuffer->publish(hi - 9, hi);
}

TEST(RingBufferWithAssertingStubTest, shouldDelegateTryNextNAndPublish) {
  auto sequencer = std::make_shared<AssertingSequencer>(16);
  auto ringBuffer = std::make_shared<disruptor::RingBuffer<disruptor::support::StubEvent>>(disruptor::support::StubEvent::EVENT_FACTORY, sequencer);
  int64_t hi = ringBuffer->tryNext(10);
  ringBuffer->publish(hi - 9, hi);
}
