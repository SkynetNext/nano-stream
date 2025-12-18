#include <gtest/gtest.h>

#include "disruptor/PhasedBackoffWaitStrategy.h"
#include "tests/disruptor/support/WaitStrategyTestUtil.h"

#include <chrono>

TEST(PhasedBackoffWaitStrategyTest, shouldHandleImmediateSequenceChange) {
  auto ws1 = disruptor::PhasedBackoffWaitStrategy<disruptor::BlockingWaitStrategy>::withLock(1'000'000, 1'000'000);
  auto ws2 = disruptor::PhasedBackoffWaitStrategy<disruptor::SleepingWaitStrategy>::withSleep(1'000'000, 1'000'000);
  EXPECT_NO_THROW(disruptor::support::WaitStrategyTestUtil::assertWaitForWithDelayOf(0, ws1));
  EXPECT_NO_THROW(disruptor::support::WaitStrategyTestUtil::assertWaitForWithDelayOf(0, ws2));
}

TEST(PhasedBackoffWaitStrategyTest, shouldHandleSequenceChangeWithOneMillisecondDelay) {
  auto ws1 = disruptor::PhasedBackoffWaitStrategy<disruptor::BlockingWaitStrategy>::withLock(1'000'000, 1'000'000);
  auto ws2 = disruptor::PhasedBackoffWaitStrategy<disruptor::SleepingWaitStrategy>::withSleep(1'000'000, 1'000'000);
  EXPECT_NO_THROW(disruptor::support::WaitStrategyTestUtil::assertWaitForWithDelayOf(1, ws1));
  EXPECT_NO_THROW(disruptor::support::WaitStrategyTestUtil::assertWaitForWithDelayOf(1, ws2));
}

TEST(PhasedBackoffWaitStrategyTest, shouldHandleSequenceChangeWithTwoMillisecondDelay) {
  auto ws1 = disruptor::PhasedBackoffWaitStrategy<disruptor::BlockingWaitStrategy>::withLock(1'000'000, 1'000'000);
  auto ws2 = disruptor::PhasedBackoffWaitStrategy<disruptor::SleepingWaitStrategy>::withSleep(1'000'000, 1'000'000);
  EXPECT_NO_THROW(disruptor::support::WaitStrategyTestUtil::assertWaitForWithDelayOf(2, ws1));
  EXPECT_NO_THROW(disruptor::support::WaitStrategyTestUtil::assertWaitForWithDelayOf(2, ws2));
}

TEST(PhasedBackoffWaitStrategyTest, shouldHandleSequenceChangeWithTenMillisecondDelay) {
  auto ws1 = disruptor::PhasedBackoffWaitStrategy<disruptor::BlockingWaitStrategy>::withLock(1'000'000, 1'000'000);
  auto ws2 = disruptor::PhasedBackoffWaitStrategy<disruptor::SleepingWaitStrategy>::withSleep(1'000'000, 1'000'000);
  EXPECT_NO_THROW(disruptor::support::WaitStrategyTestUtil::assertWaitForWithDelayOf(10, ws1));
  EXPECT_NO_THROW(disruptor::support::WaitStrategyTestUtil::assertWaitForWithDelayOf(10, ws2));
}
