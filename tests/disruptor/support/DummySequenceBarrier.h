#pragma once
// 1:1 port of com.lmax.disruptor.support.DummySequenceBarrier
// Updated for template-based architecture: no inheritance, just implements the interface

#include "disruptor/AlertException.h"

#include <cstdint>

namespace disruptor::support {

// DummySequenceBarrier implements the SequenceBarrier concept (no base class)
class DummySequenceBarrier final {
public:
  int64_t waitFor(int64_t /*sequence*/) { return 0; }
  int64_t getCursor() const { return 0; }
  bool isAlerted() const { return false; }
  void alert() {}
  void clearAlert() {}
  void checkAlert() {}
};

} // namespace disruptor::support


