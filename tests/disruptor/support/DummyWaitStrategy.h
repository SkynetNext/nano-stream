#pragma once
// 1:1 port of com.lmax.disruptor.support.DummyWaitStrategy

#include "disruptor/Sequence.h"
#include "disruptor/SequenceBarrier.h"
#include "disruptor/WaitStrategy.h"

#include <cstdint>

namespace disruptor::support {

class DummyWaitStrategy final : public ::disruptor::WaitStrategy {
public:
  int signalAllWhenBlockingCalls = 0;

  int64_t waitFor(int64_t /*sequence*/,
                  const ::disruptor::Sequence& /*cursor*/,
                  const ::disruptor::Sequence& /*dependentSequence*/,
                  ::disruptor::SequenceBarrier& /*barrier*/) override {
    return 0;
  }

  void signalAllWhenBlocking() override { ++signalAllWhenBlockingCalls; }
  bool isBlockingStrategy() const noexcept override { return true; }
};

} // namespace disruptor::support


