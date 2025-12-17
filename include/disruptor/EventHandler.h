#pragma once
// 1:1 port of com.lmax.disruptor.EventHandler
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/EventHandler.java

#include "EventHandlerBase.h"
#include "Sequence.h"

#include <cstdint>

namespace disruptor {

template <typename T>
class EventHandler : public EventHandlerBase<T> {
public:
  ~EventHandler() override = default;

  // Java throws Throwable; in C++ implementations may throw exceptions.
  void onEvent(T& event, int64_t sequence, bool endOfBatch) override = 0;

  // Java default method.
  virtual void setSequenceCallback(Sequence& /*sequenceCallback*/) {}
};

} // namespace disruptor
