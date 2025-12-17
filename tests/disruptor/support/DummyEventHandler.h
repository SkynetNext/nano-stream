#pragma once
// 1:1 port of com.lmax.disruptor.support.DummyEventHandler

#include "disruptor/EventHandler.h"

namespace disruptor::support {

template <typename T>
class DummyEventHandler final : public ::disruptor::EventHandler<T> {
public:
  int startCalls = 0;
  int shutdownCalls = 0;
  T* lastEvent = nullptr;
  int64_t lastSequence = 0;

  void onStart() override { ++startCalls; }
  void onShutdown() override { ++shutdownCalls; }

  void onEvent(T& event, int64_t sequence, bool /*endOfBatch*/) override {
    lastEvent = &event;
    lastSequence = sequence;
  }
};

} // namespace disruptor::support


