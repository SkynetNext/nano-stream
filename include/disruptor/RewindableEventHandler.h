#pragma once
// 1:1 port of com.lmax.disruptor.RewindableEventHandler
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/RewindableEventHandler.java

#include "EventHandlerBase.h"

namespace disruptor {

// Marker interface in Java; method signature differs only by allowed throw types.
template <typename T>
class RewindableEventHandler : public EventHandlerBase<T> {
public:
  ~RewindableEventHandler() override = default;
  void onEvent(T& event, int64_t sequence, bool endOfBatch) override = 0;
};

} // namespace disruptor
