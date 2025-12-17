#pragma once
// 1:1 port of com.lmax.disruptor.Sequenced
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/Sequenced.java

#include <cstdint>

namespace disruptor {

class Sequenced {
public:
  virtual ~Sequenced() = default;

  virtual int getBufferSize() const = 0;
  virtual bool hasAvailableCapacity(int requiredCapacity) = 0;
  virtual int64_t remainingCapacity() = 0;

  virtual int64_t next() = 0;
  virtual int64_t next(int n) = 0;

  virtual int64_t tryNext() = 0;
  virtual int64_t tryNext(int n) = 0;

  virtual void publish(int64_t sequence) = 0;
  virtual void publish(int64_t lo, int64_t hi) = 0;
};

} // namespace disruptor


