#include <gtest/gtest.h>

#include "disruptor/FixedSequenceGroup.h"
#include "disruptor/Sequence.h"

TEST(FixedSequenceGroupTest, shouldReturnMinimumOf2Sequences) {
  disruptor::Sequence sequence1(34);
  disruptor::Sequence sequence2(47);
  disruptor::Sequence* sequences[] = {&sequence1, &sequence2};
  disruptor::FixedSequenceGroup group(sequences, 2);

  EXPECT_EQ(34, group.get());
  sequence1.set(35);
  EXPECT_EQ(35, group.get());
  sequence1.set(48);
  EXPECT_EQ(47, group.get());
}
