#pragma once
// 1:1 port of com.lmax.disruptor.NanosecondPauseBatchRewindStrategy
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/NanosecondPauseBatchRewindStrategy.java

#include "BatchRewindStrategy.h"
#include "RewindAction.h"

#include <cstdint>
#include <thread>
#include <chrono>

namespace disruptor {

class NanosecondPauseBatchRewindStrategy final : public BatchRewindStrategy {
public:
  explicit NanosecondPauseBatchRewindStrategy(int64_t nanoSecondPauseTime)
      : nanoSecondPauseTime_(nanoSecondPauseTime) {}

  RewindAction handleRewindException(const RewindableException& /*e*/, int /*attempts*/) override {
    std::this_thread::sleep_for(std::chrono::nanoseconds(nanoSecondPauseTime_));
    return RewindAction::REWIND;
  }

private:
  int64_t nanoSecondPauseTime_;
};

} // namespace disruptor
