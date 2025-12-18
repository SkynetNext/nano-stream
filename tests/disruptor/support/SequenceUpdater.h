#pragma once
// 1:1 port of com.lmax.disruptor.support.SequenceUpdater
// Updated for template-based architecture: template on WaitStrategy type

#include "disruptor/Sequence.h"

#include <barrier>
#include <chrono>
#include <thread>

namespace disruptor::support {

template <typename WaitStrategyT>
class SequenceUpdater final {
public:
  ::disruptor::Sequence sequence;

  SequenceUpdater(int64_t sleepTimeMillis, WaitStrategyT& waitStrategy)
      : barrier_(2), sleepTimeMillis_(sleepTimeMillis), waitStrategy_(&waitStrategy) {}

  void operator()() { run(); }

  void run() {
    try {
      barrier_.arrive_and_wait();
      if (sleepTimeMillis_ != 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepTimeMillis_));
      }
      sequence.incrementAndGet();
      waitStrategy_->signalAllWhenBlocking();
    } catch (...) {
      // Match Java: swallow
    }
  }

  void waitForStartup() { barrier_.arrive_and_wait(); }

private:
  std::barrier<> barrier_;
  int64_t sleepTimeMillis_;
  WaitStrategyT* waitStrategy_;
};

} // namespace disruptor::support


