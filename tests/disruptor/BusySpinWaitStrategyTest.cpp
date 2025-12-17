#include <gtest/gtest.h>

#include "disruptor/BusySpinWaitStrategy.h"
#include "tests/disruptor/support/WaitStrategyTestUtil.h"

TEST(BusySpinWaitStrategyTest, shouldWaitForValue) {
  disruptor::BusySpinWaitStrategy waitStrategy;
  EXPECT_NO_THROW(disruptor::support::WaitStrategyTestUtil::assertWaitForWithDelayOf(50, waitStrategy));
}
