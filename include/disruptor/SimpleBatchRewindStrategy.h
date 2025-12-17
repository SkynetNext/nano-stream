#pragma once
// 1:1 port of com.lmax.disruptor.SimpleBatchRewindStrategy
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/SimpleBatchRewindStrategy.java

#include "BatchRewindStrategy.h"
#include "RewindAction.h"

namespace disruptor {

class SimpleBatchRewindStrategy final : public BatchRewindStrategy {
public:
  RewindAction handleRewindException(const RewindableException& /*e*/, int /*attempts*/) override {
    return RewindAction::REWIND;
  }
};

} // namespace disruptor
