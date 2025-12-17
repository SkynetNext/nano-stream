#include <gtest/gtest.h>

#include "disruptor/dsl/ConsumerRepository.h"
#include "tests/disruptor/support/DummyEventProcessor.h"
#include "tests/disruptor/support/DummySequenceBarrier.h"
#include "tests/disruptor/dsl/stubs/SleepingEventHandler.h"

TEST(ConsumerRepositoryTest, shouldGetBarrierByHandler) {
  disruptor::dsl::ConsumerRepository repo;
  disruptor::support::DummyEventProcessor processor1;
  processor1.run();
  disruptor::dsl::stubs::SleepingEventHandler handler1;
  disruptor::support::DummySequenceBarrier barrier1;

  repo.add(processor1, handler1, barrier1);
  EXPECT_EQ(repo.getBarrierFor(handler1), &barrier1);
}

TEST(ConsumerRepositoryTest, shouldReturnNullForBarrierWhenHandlerIsNotRegistered) {
  disruptor::dsl::ConsumerRepository repo;
  disruptor::dsl::stubs::SleepingEventHandler handler1;
  EXPECT_EQ(repo.getBarrierFor(handler1), nullptr);
}

TEST(ConsumerRepositoryTest, shouldRetrieveEventProcessorForHandler) {
  disruptor::dsl::ConsumerRepository repo;
  disruptor::support::DummyEventProcessor processor1;
  processor1.run();
  disruptor::dsl::stubs::SleepingEventHandler handler1;
  disruptor::support::DummySequenceBarrier barrier1;
  repo.add(processor1, handler1, barrier1);
  EXPECT_EQ(&repo.getEventProcessorFor(handler1), &processor1);
}

TEST(ConsumerRepositoryTest, shouldThrowExceptionWhenHandlerIsNotRegistered) {
  disruptor::dsl::ConsumerRepository repo;
  disruptor::dsl::stubs::SleepingEventHandler handler;
  disruptor::dsl::stubs::SleepingEventHandler unregistered;
  (void)handler;
  EXPECT_THROW((void)repo.getEventProcessorFor(unregistered), std::invalid_argument);
}
