#pragma once
// 1:1 port of com.lmax.disruptor.BlockingWaitStrategy
// Source:
// reference/disruptor/src/main/java/com/lmax/disruptor/BlockingWaitStrategy.java

#include "Sequence.h"
#include "WaitStrategy.h"
#include "util/ThreadHints.h"

#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace disruptor {

class BlockingWaitStrategy final {
public:
  static constexpr bool kIsBlockingStrategy = true;

  template <typename Barrier>
  int64_t waitFor(int64_t sequence, const Sequence &cursorSequence,
                  const Sequence &dependentSequence, Barrier &barrier) {
    int64_t availableSequence;
    if (cursorSequence.get() < sequence) {
      std::unique_lock<std::mutex> lock(mutex_);
      while (cursorSequence.get() < sequence) {
        barrier.checkAlert();
        cv_.wait(lock);
      }
    }

    while ((availableSequence = dependentSequence.get()) < sequence) {
      barrier.checkAlert();
      disruptor::util::ThreadHints::onSpinWait();
    }

    return availableSequence;
  }

  void signalAllWhenBlocking() {
    std::lock_guard<std::mutex> lock(mutex_);
    cv_.notify_all();
  }

  // Public accessors for WaitSpinningHelper (matches Java reflection access)
  std::mutex &getMutex() { return mutex_; }
  std::condition_variable &getConditionVariable() { return cv_; }

private:
  std::mutex mutex_;
  std::condition_variable cv_;
};

} // namespace disruptor
