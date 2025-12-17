#pragma once
// 1:1 port of com.lmax.disruptor.EventProcessor
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/EventProcessor.java

#include <functional>

namespace disruptor {

class Sequence;

class EventProcessor {
public:
  virtual ~EventProcessor() = default;

  // Java extends Runnable; we model it as a run() method.
  virtual void run() = 0;

  virtual Sequence& getSequence() = 0;
  virtual void halt() = 0;
  virtual bool isRunning() = 0;
};

} // namespace disruptor
