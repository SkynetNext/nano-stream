#pragma once
// 1:1 port skeleton of com.lmax.disruptor.WaitStrategy

#include <cstdint>

namespace disruptor {

class Sequence;
class SequenceBarrier;

class WaitStrategy {
public:
  virtual ~WaitStrategy() = default;
  virtual int64_t waitFor(int64_t sequence, const Sequence& cursor, const Sequence& dependentSequence,
                          SequenceBarrier& barrier) = 0;
  virtual void signalAllWhenBlocking() = 0;
};

} // namespace disruptor


