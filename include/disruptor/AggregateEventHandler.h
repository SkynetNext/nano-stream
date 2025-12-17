#pragma once
// 1:1 port of com.lmax.disruptor.AggregateEventHandler
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/AggregateEventHandler.java

#include "EventHandler.h"

#include <cstdint>
#include <vector>

namespace disruptor {

template <typename T>
class AggregateEventHandler final : public EventHandler<T> {
public:
  explicit AggregateEventHandler(std::vector<EventHandler<T>*> eventHandlers)
      : eventHandlers_(std::move(eventHandlers)) {}

  void onEvent(T& event, int64_t sequence, bool endOfBatch) override {
    for (auto* h : eventHandlers_) {
      if (h) {
        h->onEvent(event, sequence, endOfBatch);
      }
    }
  }

  void onStart() override {
    for (auto* h : eventHandlers_) {
      if (h) h->onStart();
    }
  }

  void onShutdown() override {
    for (auto* h : eventHandlers_) {
      if (h) h->onShutdown();
    }
  }

private:
  std::vector<EventHandler<T>*> eventHandlers_;
};

} // namespace disruptor
