#pragma once
// 1:1 port of com.lmax.disruptor.support.DummyWaitStrategy
// Updated for template-based architecture: no inheritance, just implements the interface

#include "disruptor/Sequence.h"
#include "DummySequenceBarrier.h"

#include <cstdint>

namespace disruptor::support {

// DummyWaitStrategy implements the WaitStrategy concept (no base class)
class DummyWaitStrategy final {
public:
  static constexpr bool kIsBlockingStrategy = true;
  
  int signalAllWhenBlockingCalls = 0;

  template <typename BarrierT>
  int64_t waitFor(int64_t /*sequence*/,
                  const ::disruptor::Sequence& /*cursor*/,
                  const ::disruptor::Sequence& /*dependentSequence*/,
                  BarrierT& /*barrier*/) {
    return 0;
  }

  void signalAllWhenBlocking() { ++signalAllWhenBlockingCalls; }
  bool isBlockingStrategy() const noexcept { return true; }
};

} // namespace disruptor::support


