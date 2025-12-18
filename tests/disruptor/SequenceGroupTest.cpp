#include <gtest/gtest.h>

#include "disruptor/BusySpinWaitStrategy.h"
#include "disruptor/RingBuffer.h"
#include "disruptor/Sequence.h"
#include "disruptor/SequenceGroup.h"
#include "tests/disruptor/support/TestEvent.h"

#include <limits>

TEST(SequenceGroupTest, shouldReturnMaxSequenceWhenEmptyGroup) {
  disruptor::SequenceGroup sequenceGroup;
  EXPECT_EQ(std::numeric_limits<int64_t>::max(), sequenceGroup.get());
}

TEST(SequenceGroupTest, shouldAddOneSequenceToGroup) {
  disruptor::Sequence sequence(7);
  disruptor::SequenceGroup sequenceGroup;
  sequenceGroup.add(sequence);
  EXPECT_EQ(sequence.get(), sequenceGroup.get());
}

TEST(SequenceGroupTest, shouldNotFailIfTryingToRemoveNotExistingSequence) {
  disruptor::SequenceGroup group;
  disruptor::Sequence a;
  disruptor::Sequence b;
  disruptor::Sequence c;
  group.add(a);
  group.add(b);
  EXPECT_NO_THROW((void)group.remove(c));
}

TEST(SequenceGroupTest, shouldReportTheMinimumSequenceForGroupOfTwo) {
  disruptor::Sequence sequenceThree(3);
  disruptor::Sequence sequenceSeven(7);
  disruptor::SequenceGroup sequenceGroup;
  sequenceGroup.add(sequenceSeven);
  sequenceGroup.add(sequenceThree);
  EXPECT_EQ(sequenceThree.get(), sequenceGroup.get());
}

TEST(SequenceGroupTest, shouldReportSizeOfGroup) {
  disruptor::SequenceGroup sequenceGroup;
  disruptor::Sequence s1;
  disruptor::Sequence s2;
  disruptor::Sequence s3;
  sequenceGroup.add(s1);
  sequenceGroup.add(s2);
  sequenceGroup.add(s3);
  EXPECT_EQ(3, sequenceGroup.size());
}

TEST(SequenceGroupTest, shouldRemoveSequenceFromGroup) {
  disruptor::Sequence sequenceThree(3);
  disruptor::Sequence sequenceSeven(7);
  disruptor::SequenceGroup sequenceGroup;
  sequenceGroup.add(sequenceSeven);
  sequenceGroup.add(sequenceThree);
  EXPECT_EQ(sequenceThree.get(), sequenceGroup.get());

  EXPECT_TRUE(sequenceGroup.remove(sequenceThree));
  EXPECT_EQ(sequenceSeven.get(), sequenceGroup.get());
  EXPECT_EQ(1, sequenceGroup.size());
}

TEST(SequenceGroupTest, shouldRemoveSequenceFromGroupWhereItBeenAddedMultipleTimes) {
  disruptor::Sequence sequenceThree(3);
  disruptor::Sequence sequenceSeven(7);
  disruptor::SequenceGroup sequenceGroup;
  sequenceGroup.add(sequenceThree);
  sequenceGroup.add(sequenceSeven);
  sequenceGroup.add(sequenceThree);

  EXPECT_EQ(sequenceThree.get(), sequenceGroup.get());

  EXPECT_TRUE(sequenceGroup.remove(sequenceThree));
  EXPECT_EQ(sequenceSeven.get(), sequenceGroup.get());
  EXPECT_EQ(1, sequenceGroup.size());
}

TEST(SequenceGroupTest, shouldSetGroupSequenceToSameValue) {
  disruptor::Sequence sequenceThree(3);
  disruptor::Sequence sequenceSeven(7);
  disruptor::SequenceGroup sequenceGroup;
  sequenceGroup.add(sequenceSeven);
  sequenceGroup.add(sequenceThree);

  const int64_t expectedSequence = 11;
  sequenceGroup.set(expectedSequence);

  EXPECT_EQ(expectedSequence, sequenceThree.get());
  EXPECT_EQ(expectedSequence, sequenceSeven.get());
}

TEST(SequenceGroupTest, shouldAddWhileRunning) {
  using Event = disruptor::support::TestEvent;
  using WS = disruptor::BusySpinWaitStrategy;
  using RB = disruptor::SingleProducerRingBuffer<Event, WS>;
  WS ws;
  auto ringBuffer = RB::createSingleProducer(
      disruptor::support::TestEvent::EVENT_FACTORY, 32, ws);

  disruptor::Sequence sequenceThree(3);
  disruptor::Sequence sequenceSeven(7);
  disruptor::SequenceGroup sequenceGroup;
  sequenceGroup.add(sequenceSeven);

  for (int i = 0; i < 11; ++i) {
    ringBuffer->publish(ringBuffer->next());
  }

  sequenceGroup.addWhileRunning(*ringBuffer, sequenceThree);
  EXPECT_EQ(10, sequenceThree.get());
}
