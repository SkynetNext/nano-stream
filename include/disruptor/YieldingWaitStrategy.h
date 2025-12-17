#pragma once
// 1:1 port of com.lmax.disruptor.YieldingWaitStrategy
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/YieldingWaitStrategy.java

#include "Sequence.h"
#include "SequenceBarrier.h"
#include "WaitStrategy.h"

#include <cstdint>
#include <thread>

namespace disruptor {

class YieldingWaitStrategy final : public WaitStrategy {
public:
  int64_t waitFor(int64_t sequence,
                  const Sequence& /*cursor*/,
                  const Sequence& dependentSequence,
                  SequenceBarrier& barrier) override {
    int64_t availableSequence;
    int counter = SPIN_TRIES;

    while ((availableSequence = dependentSequence.get()) < sequence) {
      counter = applyWaitMethod(barrier, counter);
    }
    return availableSequence;
  }

  void signalAllWhenBlocking() override {}

private:
  static constexpr int SPIN_TRIES = 100;

  static int applyWaitMethod(SequenceBarrier& barrier, int counter) {
    barrier.checkAlert();
    if (counter == 0) {
      std::this_thread::yield();
      return counter;
    }
    return counter - 1;
  }
};

} // namespace disruptor
