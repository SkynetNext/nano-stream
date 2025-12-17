#pragma once
// 1:1 port of com.lmax.disruptor.LiteBlockingWaitStrategy
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/LiteBlockingWaitStrategy.java

#include "Sequence.h"
#include "WaitStrategy.h"
#include "util/ThreadHints.h"

#include <atomic>
#include <cstdint>
#include <condition_variable>
#include <mutex>

namespace disruptor {

class LiteBlockingWaitStrategy final {
public:
  static constexpr bool kIsBlockingStrategy = true;

  template <typename Barrier>
  int64_t waitFor(int64_t sequence,
                  const Sequence& cursorSequence,
                  const Sequence& dependentSequence,
                  Barrier& barrier) {
    int64_t availableSequence;
    if (cursorSequence.get() < sequence) {
      std::unique_lock<std::mutex> lock(mutex_);
      do {
        signalNeeded_.store(true, std::memory_order_release);

        if (cursorSequence.get() >= sequence) {
          break;
        }

        barrier.checkAlert();
        cv_.wait(lock);
      } while (cursorSequence.get() < sequence);
    }

    while ((availableSequence = dependentSequence.get()) < sequence) {
      barrier.checkAlert();
      disruptor::util::ThreadHints::onSpinWait();
    }

    return availableSequence;
  }

  void signalAllWhenBlocking() {
    if (signalNeeded_.exchange(false, std::memory_order_acq_rel)) {
      std::lock_guard<std::mutex> lock(mutex_);
      cv_.notify_all();
    }
  }

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  std::atomic<bool> signalNeeded_{false};
};

} // namespace disruptor
