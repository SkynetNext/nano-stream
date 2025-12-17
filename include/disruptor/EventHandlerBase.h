#pragma once
// 1:1 port of com.lmax.disruptor.EventHandlerBase
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/EventHandlerBase.java

#include "EventHandlerIdentity.h"

#include <cstdint>

namespace disruptor {

// Java is package-private (@FunctionalInterface interface EventHandlerBase<T> extends EventHandlerIdentity).
// C++ port keeps it public.
template <typename T>
class EventHandlerBase : public EventHandlerIdentity {
public:
  ~EventHandlerBase() override = default;

  virtual void onEvent(T& event, int64_t sequence, bool endOfBatch) = 0;

  virtual void onBatchStart(int64_t /*batchSize*/, int64_t /*queueDepth*/) {}
  virtual void onStart() {}
  virtual void onShutdown() {}
  virtual void onTimeout(int64_t /*sequence*/) {}
};

} // namespace disruptor
