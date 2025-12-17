#pragma once
// 1:1 port of com.lmax.disruptor.EventuallyGiveUpBatchRewindStrategy
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/EventuallyGiveUpBatchRewindStrategy.java

#include "BatchRewindStrategy.h"
#include "RewindAction.h"

#include <cstdint>

namespace disruptor {

class EventuallyGiveUpBatchRewindStrategy final : public BatchRewindStrategy {
public:
  explicit EventuallyGiveUpBatchRewindStrategy(int64_t maxAttempts) : maxAttempts_(maxAttempts) {}

  RewindAction handleRewindException(const RewindableException& /*e*/, int attempts) override {
    if (attempts == maxAttempts_) {
      return RewindAction::THROW;
    }
    return RewindAction::REWIND;
  }

private:
  int64_t maxAttempts_;
};

} // namespace disruptor
