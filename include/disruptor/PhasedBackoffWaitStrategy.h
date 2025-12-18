#pragma once
// 1:1 port of com.lmax.disruptor.PhasedBackoffWaitStrategy
// Source:
// reference/disruptor/src/main/java/com/lmax/disruptor/PhasedBackoffWaitStrategy.java

#include "BlockingWaitStrategy.h"
#include "LiteBlockingWaitStrategy.h"
#include "Sequence.h"
#include "SleepingWaitStrategy.h"
#include "WaitStrategy.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>


namespace disruptor {

template <typename FallbackStrategy> class PhasedBackoffWaitStrategy final {
public:
  static constexpr bool kIsBlockingStrategy =
      FallbackStrategy::kIsBlockingStrategy;

  // Constructor that accepts an existing fallback strategy (by reference for non-movable types)
  template <typename FS = FallbackStrategy>
  PhasedBackoffWaitStrategy(int64_t spinTimeoutNanos, int64_t yieldTimeoutNanos,
                            FS&& fallbackStrategy)
      : spinTimeoutNanos_(spinTimeoutNanos),
        yieldTimeoutNanos_(spinTimeoutNanos + yieldTimeoutNanos),
        fallbackStrategy_(std::forward<FS>(fallbackStrategy)) {}

  // Private constructor for direct construction (used by static factory methods)
private:
  template <typename... Args>
  PhasedBackoffWaitStrategy(int64_t spinTimeoutNanos, int64_t yieldTimeoutNanos,
                            std::in_place_t, Args&&... args)
      : spinTimeoutNanos_(spinTimeoutNanos),
        yieldTimeoutNanos_(spinTimeoutNanos + yieldTimeoutNanos),
        fallbackStrategy_(std::forward<Args>(args)...) {}

public:
  static PhasedBackoffWaitStrategy<BlockingWaitStrategy>
  withLock(int64_t spinTimeoutNanos, int64_t yieldTimeoutNanos) {
    return PhasedBackoffWaitStrategy<BlockingWaitStrategy>(
        spinTimeoutNanos, yieldTimeoutNanos, std::in_place);
  }

  static PhasedBackoffWaitStrategy<LiteBlockingWaitStrategy>
  withLiteLock(int64_t spinTimeoutNanos, int64_t yieldTimeoutNanos) {
    return PhasedBackoffWaitStrategy<LiteBlockingWaitStrategy>(
        spinTimeoutNanos, yieldTimeoutNanos, std::in_place);
  }

  static PhasedBackoffWaitStrategy<SleepingWaitStrategy>
  withSleep(int64_t spinTimeoutNanos, int64_t yieldTimeoutNanos) {
    return PhasedBackoffWaitStrategy<SleepingWaitStrategy>(
        spinTimeoutNanos, yieldTimeoutNanos, std::in_place, 0);
  }

  template <typename Barrier>
  int64_t waitFor(int64_t sequence, const Sequence &cursor,
                  const Sequence &dependentSequence, Barrier &barrier) {
    int64_t availableSequence;
    int64_t startTimeNs = 0;
    int counter = SPIN_TRIES;

    while (true) {
      if ((availableSequence = dependentSequence.get()) >= sequence) {
        return availableSequence;
      }

      if (--counter == 0) {
        if (startTimeNs == 0) {
          startTimeNs = nowNanos();
        } else {
          const int64_t timeDelta = nowNanos() - startTimeNs;
          if (timeDelta > yieldTimeoutNanos_) {
            return fallbackStrategy_.waitFor(sequence, cursor,
                                             dependentSequence, barrier);
          } else if (timeDelta > spinTimeoutNanos_) {
            std::this_thread::yield();
          }
        }
        counter = SPIN_TRIES;
      }
    }
  }

  void signalAllWhenBlocking() { fallbackStrategy_.signalAllWhenBlocking(); }

private:
  static constexpr int SPIN_TRIES = 10000;
  int64_t spinTimeoutNanos_;
  int64_t yieldTimeoutNanos_;
  FallbackStrategy fallbackStrategy_;

  static int64_t nowNanos() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }
};

} // namespace disruptor
