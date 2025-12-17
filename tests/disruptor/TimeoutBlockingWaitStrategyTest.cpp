#include <gtest/gtest.h>

#include "disruptor/Sequence.h"
#include "disruptor/TimeoutBlockingWaitStrategy.h"
#include "disruptor/TimeoutException.h"
#include "tests/disruptor/support/DummySequenceBarrier.h"

#include <chrono>

TEST(TimeoutBlockingWaitStrategyTest, shouldTimeoutWaitFor) {
  disruptor::support::DummySequenceBarrier sequenceBarrier;

  const int64_t theTimeoutMillis = 500;
  const int64_t timeoutNanos = theTimeoutMillis * 1'000'000;
  disruptor::TimeoutBlockingWaitStrategy waitStrategy(timeoutNanos);
  disruptor::Sequence cursor(5);

  const auto t0 = std::chrono::steady_clock::now();

  EXPECT_THROW(
      { (void)waitStrategy.waitFor(6, cursor, cursor, sequenceBarrier); },
      disruptor::TimeoutException);

  const auto t1 = std::chrono::steady_clock::now();
  const auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
  EXPECT_GE(waited, theTimeoutMillis);
}
