#include <gtest/gtest.h>

#include "disruptor/YieldingWaitStrategy.h"
#include "tests/disruptor/support/WaitStrategyTestUtil.h"

TEST(YieldingWaitStrategyTest, shouldWaitForValue) {
  disruptor::YieldingWaitStrategy waitStrategy;
  EXPECT_NO_THROW(disruptor::support::WaitStrategyTestUtil::assertWaitForWithDelayOf(50, waitStrategy));
}
