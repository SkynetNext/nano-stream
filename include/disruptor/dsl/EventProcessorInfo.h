#pragma once
// 1:1 port of com.lmax.disruptor.dsl.EventProcessorInfo (package-private class)
// Source:
// reference/disruptor/src/main/java/com/lmax/disruptor/dsl/EventProcessorInfo.java

#include "../EventProcessor.h"
#include "../Sequence.h"
#include "ConsumerInfo.h"
#include "ThreadFactory.h"

#include <latch>
#include <thread>

namespace disruptor::dsl {

template <typename BarrierPtrT>
class EventProcessorInfo final : public ConsumerInfo<BarrierPtrT> {
public:
  EventProcessorInfo(EventProcessor &eventprocessor, BarrierPtrT barrier)
      : eventprocessor_(&eventprocessor), barrier_(barrier), endOfChain_(true) {
  }

  EventProcessor &getEventProcessor() { return *eventprocessor_; }

  // Java version: return new Sequence[]{eventprocessor.getSequence()};
  // C++ version: use static thread_local array to align with Java's dynamic
  // array creation and avoid data races (each thread has its own array).
  Sequence *const *getSequences() override {
    static thread_local Sequence *sequences[1];
    sequences[0] = &eventprocessor_->getSequence();
    return sequences;
  }
  int getSequenceCount() const override { return 1; }

  BarrierPtrT getBarrier() override { return barrier_; }
  bool isEndOfChain() override { return endOfChain_; }

  void start(ThreadFactory &threadFactory,
             std::latch *startupLatch = nullptr) override {
    // Java: Thread thread = threadFactory.newThread(eventprocessor);
    // thread.start(); C++: std::thread starts immediately; we must keep it and
    // join on shutdown to avoid use-after-free.
    EventProcessor *ep = eventprocessor_;
    if (startupLatch) {
      // Signal latch when thread starts (C++20 std::latch for startup
      // synchronization)
      // Critical: count_down() must be called even if thread creation fails
      // to prevent deadlock in Startup()
      try {
        thread_ = threadFactory.newThread([ep, startupLatch] {
          startupLatch->count_down(); // Signal that this thread has started
          ep->run();                  // Execute the processor
        });
      } catch (...) {
        // If thread creation fails, still signal latch to prevent deadlock
        startupLatch->count_down();
        throw; // Re-throw to propagate the error
      }
    } else {
      thread_ = threadFactory.newThread([ep] { ep->run(); });
    }
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
  EventProcessor *eventprocessor_;
  BarrierPtrT barrier_;
  bool endOfChain_;
  std::thread thread_{};
};

} // namespace disruptor::dsl
