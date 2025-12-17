#pragma once
// 1:1 port skeleton of com.lmax.disruptor.SequenceBarrier

#include <cstdint>

namespace disruptor {

class SequenceBarrier {
public:
  virtual ~SequenceBarrier() = default;
  virtual int64_t waitFor(int64_t sequence) = 0;
  virtual int64_t getCursor() const = 0;
  virtual bool isAlerted() const = 0;
  virtual void alert() = 0;
  virtual void clearAlert() = 0;
  virtual void checkAlert() = 0;
};

} // namespace disruptor


