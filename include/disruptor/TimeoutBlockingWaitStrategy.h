#pragma once
// 1:1 port of com.lmax.disruptor.TimeoutBlockingWaitStrategy
// Source:
// reference/disruptor/src/main/java/com/lmax/disruptor/TimeoutBlockingWaitStrategy.java

#include "Sequence.h"
#include "TimeoutException.h"
#include "WaitStrategy.h"
#include "util/Util.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace disruptor {

class TimeoutBlockingWaitStrategy final {
public:
  static constexpr bool kIsBlockingStrategy = true;

  // Java takes (timeout, TimeUnit). C++ port takes an already-converted
  // nanoseconds value.
  explicit TimeoutBlockingWaitStrategy(int64_t timeoutInNanos)
      : timeoutInNanos_(timeoutInNanos) {}

  template <typename Barrier>
  int64_t waitFor(int64_t sequence, const Sequence &cursorSequence,
                  const Sequence &dependentSequence, Barrier &barrier) {
    int64_t timeoutNanos = timeoutInNanos_;

    int64_t availableSequence;
    if (cursorSequence.get() < sequence) {
      std::unique_lock<std::mutex> lock(mutex_);
      while (cursorSequence.get() < sequence) {
        barrier.checkAlert();
        timeoutNanos =
            disruptor::util::Util::awaitNanos(cv_, lock, timeoutNanos);
        if (timeoutNanos <= 0) {
          throw TimeoutException::INSTANCE();
        }
      }
    }

    while ((availableSequence = dependentSequence.get()) < sequence) {
      barrier.checkAlert();
    }

    return availableSequence;
  }

  void signalAllWhenBlocking() {
    std::lock_guard<std::mutex> lock(mutex_);
    cv_.notify_all();
  }

private:
  std::mutex mutex_;
  std::condition_variable cv_;
  int64_t timeoutInNanos_;
};

} // namespace disruptor
