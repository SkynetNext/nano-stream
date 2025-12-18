#include <gtest/gtest.h>

#include "disruptor/InsufficientCapacityException.h"
#include "disruptor/RingBuffer.h"
#include "disruptor/Sequencer.h"
#include "tests/disruptor/support/StubEvent.h"

#include <random>

namespace {
// AssertingSequencer implements Sequencer concept (no inheritance)
class AssertingSequencer final {
public:
  explicit AssertingSequencer(int size) : size_(size) {}

  int getBufferSize() const { return size_; }
  bool hasAvailableCapacity(int requiredCapacity) { return requiredCapacity <= size_; }
  int64_t remainingCapacity() { return size_; }

  int64_t next() {
    lastValue_ = dist_(rng_);
    lastBatchSize_ = 1;
    return lastValue_;
  }
  int64_t next(int n) {
    lastValue_ = std::max<int64_t>(n, dist_(rng_));
    lastBatchSize_ = n;
    return lastValue_;
  }

  int64_t tryNext() { return next(); }
  int64_t tryNext(int n) { return next(n); }

  void publish(int64_t sequence) {
    EXPECT_EQ(sequence, lastValue_);
    EXPECT_EQ(lastBatchSize_, 1);
  }
  void publish(int64_t lo, int64_t hi) {
    EXPECT_EQ(hi, lastValue_);
    EXPECT_EQ((hi - lo) + 1, lastBatchSize_);
  }

  int64_t getCursor() const { return lastValue_; }
  void claim(int64_t /*sequence*/) {}
  bool isAvailable(int64_t /*sequence*/) { return false; }
  void addGatingSequences(disruptor::Sequence* const* /*gatingSequences*/, int /*count*/) {}
  bool removeGatingSequence(disruptor::Sequence& /*sequence*/) { return false; }
  std::shared_ptr<disruptor::SequenceBarrier> newBarrier(disruptor::Sequence* const* /*sequencesToTrack*/, int /*count*/) {
    return nullptr;
  }
  int64_t getMinimumSequence() { return 0; }
  int64_t getHighestPublishedSequence(int64_t /*nextSequence*/, int64_t /*availableSequence*/) { return 0; }

private:
  int size_;
  int64_t lastBatchSize_{-1};
  int64_t lastValue_{-1};
  std::mt19937_64 rng_{123};
  std::uniform_int_distribution<int64_t> dist_{0, 1'000'000};
};
} // namespace

TEST(RingBufferWithAssertingStubTest, shouldDelegateNextAndPublish) {
  using Event = disruptor::support::StubEvent;
  auto sequencer = std::make_unique<AssertingSequencer>(16);
  auto ringBuffer = std::make_shared<disruptor::RingBuffer<Event, AssertingSequencer>>(
      disruptor::support::StubEvent::EVENT_FACTORY, std::move(sequencer));
  ringBuffer->publish(ringBuffer->next());
}

TEST(RingBufferWithAssertingStubTest, shouldDelegateTryNextAndPublish) {
  using Event = disruptor::support::StubEvent;
  auto sequencer = std::make_unique<AssertingSequencer>(16);
  auto ringBuffer = std::make_shared<disruptor::RingBuffer<Event, AssertingSequencer>>(
      disruptor::support::StubEvent::EVENT_FACTORY, std::move(sequencer));
  ringBuffer->publish(ringBuffer->tryNext());
}

TEST(RingBufferWithAssertingStubTest, shouldDelegateNextNAndPublish) {
  using Event = disruptor::support::StubEvent;
  auto sequencer = std::make_unique<AssertingSequencer>(16);
  auto ringBuffer = std::make_shared<disruptor::RingBuffer<Event, AssertingSequencer>>(
      disruptor::support::StubEvent::EVENT_FACTORY, std::move(sequencer));
  int64_t hi = ringBuffer->next(10);
  ringBuffer->publish(hi - 9, hi);
}

TEST(RingBufferWithAssertingStubTest, shouldDelegateTryNextNAndPublish) {
  using Event = disruptor::support::StubEvent;
  auto sequencer = std::make_unique<AssertingSequencer>(16);
  auto ringBuffer = std::make_shared<disruptor::RingBuffer<Event, AssertingSequencer>>(
      disruptor::support::StubEvent::EVENT_FACTORY, std::move(sequencer));
  int64_t hi = ringBuffer->tryNext(10);
  ringBuffer->publish(hi - 9, hi);
}
