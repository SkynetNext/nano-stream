#pragma once
// 1:1 port of com.lmax.disruptor.LiteTimeoutBlockingWaitStrategy
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/LiteTimeoutBlockingWaitStrategy.java

#include "Sequence.h"
#include "SequenceBarrier.h"
#include "TimeoutException.h"
#include "WaitStrategy.h"
#include "util/Util.h"

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <mutex>

namespace disruptor {

class LiteTimeoutBlockingWaitStrategy final : public WaitStrategy {
public:
  explicit LiteTimeoutBlockingWaitStrategy(int64_t timeoutInNanos)
      : timeoutInNanos_(timeoutInNanos) {}

  int64_t waitFor(int64_t sequence,
                  const Sequence& cursorSequence,
                  const Sequence& dependentSequence,
                  SequenceBarrier& barrier) override {
    int64_t nanos = timeoutInNanos_;

    int64_t availableSequence;
    if (cursorSequence.get() < sequence) {
      std::unique_lock<std::mutex> lock(mutex_);
      while (cursorSequence.get() < sequence) {
        signalNeeded_.store(true, std::memory_order_release);
        barrier.checkAlert();
        nanos = disruptor::util::Util::awaitNanos(cv_, lock, nanos);
        if (nanos <= 0) {
          throw TimeoutException::INSTANCE();
        }
      }
    }

    while ((availableSequence = dependentSequence.get()) < sequence) {
      barrier.checkAlert();
    }
    return availableSequence;
  }

  void signalAllWhenBlocking() override {
    if (signalNeeded_.exchange(false, std::memory_order_acq_rel)) {
      std::lock_guard<std::mutex> lock(mutex_);
      cv_.notify_all();
    }
  }

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<bool> signalNeeded_{false};
  int64_t timeoutInNanos_;
};

} // namespace disruptor
