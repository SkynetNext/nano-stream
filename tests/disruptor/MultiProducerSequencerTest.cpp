#include <gtest/gtest.h>

#include "disruptor/BlockingWaitStrategy.h"
#include "disruptor/MultiProducerSequencer.h"

TEST(MultiProducerSequencerTest, shouldOnlyAllowMessagesToBeAvailableIfSpecificallyPublished) {
  auto ws = std::make_shared<disruptor::BlockingWaitStrategy>();
  disruptor::MultiProducerSequencer publisher(1024, ws);

  publisher.publish(3);
  publisher.publish(5);

  EXPECT_FALSE(publisher.isAvailable(0));
  EXPECT_FALSE(publisher.isAvailable(1));
  EXPECT_FALSE(publisher.isAvailable(2));
  EXPECT_TRUE(publisher.isAvailable(3));
  EXPECT_FALSE(publisher.isAvailable(4));
  EXPECT_TRUE(publisher.isAvailable(5));
  EXPECT_FALSE(publisher.isAvailable(6));
}
