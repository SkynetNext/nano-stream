#pragma once
// 1:1 port of com.lmax.disruptor.dsl.EventProcessorInfo (package-private class)
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/dsl/EventProcessorInfo.java

#include "../EventProcessor.h"
#include "../Sequence.h"
#include "ConsumerInfo.h"
#include "ThreadFactory.h"

#include <stdexcept>
#include <thread>

namespace disruptor::dsl {

template <typename BarrierPtrT>
class EventProcessorInfo final : public ConsumerInfo<BarrierPtrT> {
public:
  EventProcessorInfo(EventProcessor& eventprocessor, BarrierPtrT barrier)
      : eventprocessor_(&eventprocessor), barrier_(barrier), endOfChain_(true) {}

  EventProcessor& getEventProcessor() { return *eventprocessor_; }

  // Java version: return new Sequence[]{eventprocessor.getSequence()};
  // C++ version: dynamically update sequences_ array on each call to ensure
  // pointer validity (aligns with Java's dynamic array creation).
  Sequence* const* getSequences() override {
    sequences_[0] = &eventprocessor_->getSequence();
    return sequences_;
  }
  int getSequenceCount() const override { return 1; }

  BarrierPtrT getBarrier() override { return barrier_; }
  bool isEndOfChain() override { return endOfChain_; }

  void start(ThreadFactory& threadFactory) override {
    // Java: Thread thread = threadFactory.newThread(eventprocessor); thread.start();
    // C++: std::thread starts immediately; we must keep it and join on shutdown to avoid use-after-free.
    EventProcessor* ep = eventprocessor_;
    thread_ = threadFactory.newThread([ep] { ep->run(); });
  }

  void halt() override { eventprocessor_->halt(); }

  void join() override {
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  void markAsUsedInBarrier() override { endOfChain_ = false; }

  bool isRunning() override { return eventprocessor_->isRunning(); }

private:
  EventProcessor* eventprocessor_;
  BarrierPtrT barrier_;
  bool endOfChain_;
  // Dynamically updated on each getSequences() call to ensure pointer validity,
  // aligning with Java version which creates new array each time.
  Sequence* sequences_[1];
  std::thread thread_{};
};

} // namespace disruptor::dsl


