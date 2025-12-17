#include <gtest/gtest.h>

#include "disruptor/SleepingWaitStrategy.h"
#include "tests/disruptor/support/WaitStrategyTestUtil.h"

TEST(SleepingWaitStrategyTest, shouldWaitForValue) {
  disruptor::SleepingWaitStrategy waitStrategy;
  EXPECT_NO_THROW(disruptor::support::WaitStrategyTestUtil::assertWaitForWithDelayOf(50, waitStrategy));
}
