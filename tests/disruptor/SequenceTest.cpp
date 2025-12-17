#include <gtest/gtest.h>

#include "disruptor/Sequence.h"

TEST(SequenceTest, shouldReturnChangedValueAfterAddAndGet) {
  disruptor::Sequence sequence(0);
  EXPECT_EQ(10, sequence.addAndGet(10));
  EXPECT_EQ(10, sequence.get());
}

TEST(SequenceTest, shouldReturnIncrementedValueAfterIncrementAndGet) {
  disruptor::Sequence sequence(0);
  EXPECT_EQ(1, sequence.incrementAndGet());
  EXPECT_EQ(1, sequence.get());
}

TEST(SequenceTest, shouldReturnPreviousValueAfterGetAndAdd) {
  disruptor::Sequence sequence(0);
  EXPECT_EQ(0, sequence.getAndAdd(1));
  EXPECT_EQ(1, sequence.get());
}
