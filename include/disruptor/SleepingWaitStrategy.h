#pragma once
// 1:1 port of com.lmax.disruptor.SleepingWaitStrategy
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/SleepingWaitStrategy.java

#include "Sequence.h"
#include "SequenceBarrier.h"
#include "WaitStrategy.h"

#include <cstdint>
#include <thread>
#include <chrono>

namespace disruptor {

class SleepingWaitStrategy final : public WaitStrategy {
public:
  SleepingWaitStrategy() : SleepingWaitStrategy(DEFAULT_RETRIES, DEFAULT_SLEEP) {}
  explicit SleepingWaitStrategy(int retries) : SleepingWaitStrategy(retries, DEFAULT_SLEEP) {}
  SleepingWaitStrategy(int retries, int64_t sleepTimeNs) : retries_(retries), sleepTimeNs_(sleepTimeNs) {}

  int64_t waitFor(int64_t sequence,
                  const Sequence& /*cursor*/,
                  const Sequence& dependentSequence,
                  SequenceBarrier& barrier) override {
    int64_t availableSequence;
    int counter = retries_;

    while ((availableSequence = dependentSequence.get()) < sequence) {
      counter = applyWaitMethod(barrier, counter);
    }
    return availableSequence;
  }

  void signalAllWhenBlocking() override {}

private:
  static constexpr int SPIN_THRESHOLD = 100;
  static constexpr int DEFAULT_RETRIES = 200;
  static constexpr int64_t DEFAULT_SLEEP = 100;

  int retries_;
  int64_t sleepTimeNs_;

  int applyWaitMethod(SequenceBarrier& barrier, int counter) {
    barrier.checkAlert();

    if (counter > SPIN_THRESHOLD) {
      return counter - 1;
    } else if (counter > 0) {
      std::this_thread::yield();
      return counter - 1;
    } else {
      std::this_thread::sleep_for(std::chrono::nanoseconds(sleepTimeNs_));
      return counter;
    }
  }
};

} // namespace disruptor
