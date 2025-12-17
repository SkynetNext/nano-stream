#pragma once
// 1:1 port of com.lmax.disruptor.support.DummySequenceBarrier

#include "disruptor/AlertException.h"
#include "disruptor/SequenceBarrier.h"

namespace disruptor::support {

class DummySequenceBarrier final : public ::disruptor::SequenceBarrier {
public:
  int64_t waitFor(int64_t /*sequence*/) override { return 0; }
  int64_t getCursor() const override { return 0; }
  bool isAlerted() const override { return false; }
  void alert() override {}
  void clearAlert() override {}
  void checkAlert() override {}
};

} // namespace disruptor::support


