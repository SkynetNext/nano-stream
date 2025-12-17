#include <gtest/gtest.h>

#include "disruptor/Sequence.h"
#include "disruptor/util/Util.h"

#include <limits>

TEST(UtilTest, shouldReturnNextPowerOfTwo) {
  int powerOfTwo = disruptor::util::Util::ceilingNextPowerOfTwo(1000);
  EXPECT_EQ(1024, powerOfTwo);
}

TEST(UtilTest, shouldReturnExactPowerOfTwo) {
  int powerOfTwo = disruptor::util::Util::ceilingNextPowerOfTwo(1024);
  EXPECT_EQ(1024, powerOfTwo);
}

TEST(UtilTest, shouldReturnMinimumSequence) {
  disruptor::Sequence s1(7);
  disruptor::Sequence s2(3);
  disruptor::Sequence s3(12);
  std::vector<disruptor::Sequence*> sequences{&s1, &s2, &s3};
  EXPECT_EQ(3, disruptor::util::Util::getMinimumSequence(sequences));
}

TEST(UtilTest, shouldReturnLongMaxWhenNoEventProcessors) {
  std::vector<disruptor::Sequence*> sequences{};
  EXPECT_EQ(std::numeric_limits<int64_t>::max(), disruptor::util::Util::getMinimumSequence(sequences));
}

TEST(UtilTest, shouldThrowErrorIfValuePassedToLog2FunctionIsNotPositive) {
  EXPECT_THROW((void)disruptor::util::Util::log2(0), std::invalid_argument);
  EXPECT_THROW((void)disruptor::util::Util::log2(-1), std::invalid_argument);
  EXPECT_THROW((void)disruptor::util::Util::log2(std::numeric_limits<int>::min()), std::invalid_argument);
}

TEST(UtilTest, shouldCalculateCorrectlyIntegerFlooredLog2) {
  EXPECT_EQ(0, disruptor::util::Util::log2(1));
  EXPECT_EQ(1, disruptor::util::Util::log2(2));
  EXPECT_EQ(1, disruptor::util::Util::log2(3));
  EXPECT_EQ(10, disruptor::util::Util::log2(1024));
  EXPECT_EQ(30, disruptor::util::Util::log2(std::numeric_limits<int>::max()));
}
