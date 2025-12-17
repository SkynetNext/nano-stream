#include <gtest/gtest.h>

#include "disruptor/BusySpinWaitStrategy.h"
#include "disruptor/SingleProducerSequencer.h"

TEST(SingleProducerSequencerTest, shouldNotUpdateCursorDuringHasAvailableCapacity) {
  auto waitStrategy = std::make_unique<disruptor::BusySpinWaitStrategy>();
  disruptor::SingleProducerSequencer sequencer(16, std::move(waitStrategy));

  for (int i = 0; i < 32; ++i) {
    const int64_t next = sequencer.next();
    EXPECT_NE(sequencer.getCursor(), next);

    (void)sequencer.hasAvailableCapacity(13);
    EXPECT_NE(sequencer.getCursor(), next);

    sequencer.publish(next);
  }
}
