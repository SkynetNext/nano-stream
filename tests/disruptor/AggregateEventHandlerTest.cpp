#include <gtest/gtest.h>

#include "disruptor/AggregateEventHandler.h"
#include "tests/disruptor/support/DummyEventHandler.h"

TEST(AggregateEventHandlerTest, shouldCallOnEventInSequence) {
  disruptor::support::DummyEventHandler<int> eh1;
  disruptor::support::DummyEventHandler<int> eh2;
  disruptor::support::DummyEventHandler<int> eh3;

  int event = 7;
  const int64_t sequence = 3;
  const bool endOfBatch = true;

  std::vector<disruptor::EventHandler<int>*> handlers{&eh1, &eh2, &eh3};
  disruptor::AggregateEventHandler<int> aggregateEventHandler(std::move(handlers));

  aggregateEventHandler.onEvent(event, sequence, endOfBatch);

  ASSERT_NE(eh1.lastEvent, nullptr);
  EXPECT_EQ(*eh1.lastEvent, event);
  EXPECT_EQ(eh1.lastSequence, sequence);
  ASSERT_NE(eh2.lastEvent, nullptr);
  EXPECT_EQ(*eh2.lastEvent, event);
  EXPECT_EQ(eh2.lastSequence, sequence);
  ASSERT_NE(eh3.lastEvent, nullptr);
  EXPECT_EQ(*eh3.lastEvent, event);
  EXPECT_EQ(eh3.lastSequence, sequence);
}

TEST(AggregateEventHandlerTest, shouldCallOnStartInSequence) {
  disruptor::support::DummyEventHandler<int> eh1;
  disruptor::support::DummyEventHandler<int> eh2;
  disruptor::support::DummyEventHandler<int> eh3;
  std::vector<disruptor::EventHandler<int>*> handlers{&eh1, &eh2, &eh3};
  disruptor::AggregateEventHandler<int> aggregateEventHandler(std::move(handlers));

  aggregateEventHandler.onStart();

  EXPECT_EQ(eh1.startCalls, 1);
  EXPECT_EQ(eh2.startCalls, 1);
  EXPECT_EQ(eh3.startCalls, 1);
}

TEST(AggregateEventHandlerTest, shouldCallOnShutdownInSequence) {
  disruptor::support::DummyEventHandler<int> eh1;
  disruptor::support::DummyEventHandler<int> eh2;
  disruptor::support::DummyEventHandler<int> eh3;
  std::vector<disruptor::EventHandler<int>*> handlers{&eh1, &eh2, &eh3};
  disruptor::AggregateEventHandler<int> aggregateEventHandler(std::move(handlers));

  aggregateEventHandler.onShutdown();

  EXPECT_EQ(eh1.shutdownCalls, 1);
  EXPECT_EQ(eh2.shutdownCalls, 1);
  EXPECT_EQ(eh3.shutdownCalls, 1);
}

TEST(AggregateEventHandlerTest, shouldHandleEmptyListOfEventHandlers) {
  std::vector<disruptor::EventHandler<int>*> handlers{};
  disruptor::AggregateEventHandler<int> aggregateEventHandler(std::move(handlers));

  int event = 7;
  EXPECT_NO_THROW(aggregateEventHandler.onEvent(event, 0, true));
  EXPECT_NO_THROW(aggregateEventHandler.onStart());
  EXPECT_NO_THROW(aggregateEventHandler.onShutdown());
}
