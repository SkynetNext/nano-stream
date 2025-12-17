#pragma once
// 1:1 port of com.lmax.disruptor.PhasedBackoffWaitStrategy
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/PhasedBackoffWaitStrategy.java

#include "BlockingWaitStrategy.h"
#include "LiteBlockingWaitStrategy.h"
#include "Sequence.h"
#include "SequenceBarrier.h"
#include "SleepingWaitStrategy.h"
#include "WaitStrategy.h"

#include <cstdint>
#include <memory>
#include <thread>
#include <chrono>

namespace disruptor {

class PhasedBackoffWaitStrategy final : public WaitStrategy {
public:
  PhasedBackoffWaitStrategy(int64_t spinTimeoutNanos,
                            int64_t yieldTimeoutNanos,
                            std::shared_ptr<WaitStrategy> fallbackStrategy)
      : spinTimeoutNanos_(spinTimeoutNanos),
        yieldTimeoutNanos_(spinTimeoutNanos + yieldTimeoutNanos),
        fallbackStrategy_(std::move(fallbackStrategy)) {}

  static std::shared_ptr<PhasedBackoffWaitStrategy> withLock(int64_t spinTimeoutNanos, int64_t yieldTimeoutNanos) {
    return std::make_shared<PhasedBackoffWaitStrategy>(spinTimeoutNanos,
                                                       yieldTimeoutNanos,
                                                       std::make_shared<BlockingWaitStrategy>());
  }

  static std::shared_ptr<PhasedBackoffWaitStrategy> withLiteLock(int64_t spinTimeoutNanos, int64_t yieldTimeoutNanos) {
    return std::make_shared<PhasedBackoffWaitStrategy>(spinTimeoutNanos,
                                                       yieldTimeoutNanos,
                                                       std::make_shared<LiteBlockingWaitStrategy>());
  }

  static std::shared_ptr<PhasedBackoffWaitStrategy> withSleep(int64_t spinTimeoutNanos, int64_t yieldTimeoutNanos) {
    return std::make_shared<PhasedBackoffWaitStrategy>(spinTimeoutNanos,
                                                       yieldTimeoutNanos,
                                                       std::make_shared<SleepingWaitStrategy>(0));
  }

  int64_t waitFor(int64_t sequence,
                  const Sequence& cursor,
                  const Sequence& dependentSequence,
                  SequenceBarrier& barrier) override {
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
            return fallbackStrategy_->waitFor(sequence, cursor, dependentSequence, barrier);
          } else if (timeDelta > spinTimeoutNanos_) {
            std::this_thread::yield();
          }
        }
        counter = SPIN_TRIES;
      }
    }
  }

  void signalAllWhenBlocking() override { fallbackStrategy_->signalAllWhenBlocking(); }

private:
  static constexpr int SPIN_TRIES = 10000;
  int64_t spinTimeoutNanos_;
  int64_t yieldTimeoutNanos_;
  std::shared_ptr<WaitStrategy> fallbackStrategy_;

  static int64_t nowNanos() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
  }
};

} // namespace disruptor
