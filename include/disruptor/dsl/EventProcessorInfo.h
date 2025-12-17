#pragma once
// 1:1 port of com.lmax.disruptor.dsl.EventProcessorInfo (package-private class)
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/dsl/EventProcessorInfo.java

#include "../EventProcessor.h"
#include "../Sequence.h"
#include "../SequenceBarrier.h"
#include "ConsumerInfo.h"
#include "ThreadFactory.h"

#include <stdexcept>
#include <thread>

namespace disruptor::dsl {

class EventProcessorInfo final : public ConsumerInfo {
public:
  EventProcessorInfo(EventProcessor& eventprocessor, SequenceBarrier* barrier)
      : eventprocessor_(&eventprocessor), barrier_(barrier), endOfChain_(true) {}

  EventProcessor& getEventProcessor() { return *eventprocessor_; }

  Sequence* const* getSequences() override { return sequences_; }
  int getSequenceCount() const override { return 1; }

  SequenceBarrier* getBarrier() override { return barrier_; }
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
  SequenceBarrier* barrier_;
  bool endOfChain_;
  Sequence* sequences_[1]{&eventprocessor_->getSequence()};
  std::thread thread_{};
};

} // namespace disruptor::dsl


