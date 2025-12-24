#include <gtest/gtest.h>
#include <memory>

#include "disruptor/dsl/ConsumerRepository.h"
#include "tests/disruptor/support/DummyEventProcessor.h"
#include "tests/disruptor/support/DummySequenceBarrier.h"
#include "tests/disruptor/dsl/stubs/SleepingEventHandler.h"

TEST(ConsumerRepositoryTest, shouldGetBarrierByHandler) {
  using BarrierPtrT = disruptor::support::DummySequenceBarrier*;
  disruptor::dsl::ConsumerRepository<BarrierPtrT> repo;
  disruptor::support::DummyEventProcessor processor1;
  processor1.run();
  disruptor::dsl::stubs::SleepingEventHandler handler1;
  disruptor::support::DummySequenceBarrier barrier1;

  // Wrap in shared_ptr with no-op deleter since processor1 is on stack
  auto processorPtr = std::shared_ptr<disruptor::EventProcessor>(&processor1, [](disruptor::EventProcessor*){});
  repo.add(processorPtr, handler1, &barrier1);
  EXPECT_EQ(repo.getBarrierFor(handler1), &barrier1);
}

TEST(ConsumerRepositoryTest, shouldReturnNullForBarrierWhenHandlerIsNotRegistered) {
  using BarrierPtrT = disruptor::support::DummySequenceBarrier*;
  disruptor::dsl::ConsumerRepository<BarrierPtrT> repo;
  disruptor::dsl::stubs::SleepingEventHandler handler1;
  EXPECT_EQ(repo.getBarrierFor(handler1), nullptr);
}

TEST(ConsumerRepositoryTest, shouldRetrieveEventProcessorForHandler) {
  using BarrierPtrT = disruptor::support::DummySequenceBarrier*;
  disruptor::dsl::ConsumerRepository<BarrierPtrT> repo;
  disruptor::support::DummyEventProcessor processor1;
  processor1.run();
  disruptor::dsl::stubs::SleepingEventHandler handler1;
  disruptor::support::DummySequenceBarrier barrier1;
  // Wrap in shared_ptr with no-op deleter since processor1 is on stack
  auto processorPtr = std::shared_ptr<disruptor::EventProcessor>(&processor1, [](disruptor::EventProcessor*){});
  repo.add(processorPtr, handler1, &barrier1);
  EXPECT_EQ(&repo.getEventProcessorFor(handler1), &processor1);
}

TEST(ConsumerRepositoryTest, shouldThrowExceptionWhenHandlerIsNotRegistered) {
  using BarrierPtrT = disruptor::support::DummySequenceBarrier*;
  disruptor::dsl::ConsumerRepository<BarrierPtrT> repo;
  disruptor::dsl::stubs::SleepingEventHandler handler;
  disruptor::dsl::stubs::SleepingEventHandler unregistered;
  (void)handler;
  EXPECT_THROW((void)repo.getEventProcessorFor(unregistered), std::invalid_argument);
}
