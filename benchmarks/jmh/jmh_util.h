#pragma once
// Shared types used by multiple JMH benchmark ports.

#include "disruptor/EventFactory.h"
#include "disruptor/EventHandler.h"
#include "disruptor/EventTranslator.h"

#include <benchmark/benchmark.h>

#include <cstdint>

namespace nano_stream::bench::jmh {

// Java: reference/disruptor/src/jmh/java/com/lmax/disruptor/util/SimpleEvent.java
struct SimpleEvent {
  int64_t value{0};
};

struct SimpleEventFactory final : public disruptor::EventFactory<SimpleEvent> {
  SimpleEvent newInstance() override { return SimpleEvent(); }
};

// Equivalent of com.lmax.disruptor.util.SimpleEventHandler (consumes via Blackhole).
struct ConsumeHandler final : public disruptor::EventHandler<SimpleEvent> {
  void onEvent(SimpleEvent& e, int64_t /*sequence*/, bool /*endOfBatch*/) override {
    benchmark::DoNotOptimize(e.value);
  }
};

// Equivalent of setValue(0)
struct SetZeroTranslator final : public disruptor::EventTranslator<SimpleEvent> {
  void translateTo(SimpleEvent& event, int64_t /*sequence*/) override { event.value = 0; }
};

} // namespace nano_stream::bench::jmh


