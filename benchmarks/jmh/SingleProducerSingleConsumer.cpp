#include <benchmark/benchmark.h>

#include "jmh_config.h"
#include "jmh_util.h"

#include "disruptor/BusySpinWaitStrategy.h"
#include "disruptor/RingBuffer.h"

#include <memory>

namespace {
// Java: reference/disruptor/src/jmh/java/com/lmax/disruptor/util/Constants.java
constexpr int kRingBufferSize = 1 << 20;
} // namespace

// 1:1 with Java JMH class:
// reference/disruptor/src/jmh/java/com/lmax/disruptor/SingleProducerSingleConsumer.java
static void JMH_SingleProducerSingleConsumer_producing(benchmark::State& state) {
  const auto wait_entries_before =
      disruptor::sp_wrap_wait_entries().load(std::memory_order_relaxed);
  const auto wait_loops_before =
      disruptor::sp_wrap_wait_loops().load(std::memory_order_relaxed);

  disruptor::BusySpinWaitStrategy ws;
  auto factory = std::make_shared<nano_stream::bench::jmh::SimpleEventFactory>();

  auto rb = disruptor::RingBuffer<nano_stream::bench::jmh::SimpleEvent,
                                  disruptor::SingleProducerSequencer<disruptor::BusySpinWaitStrategy>>::
      createSingleProducer(factory, kRingBufferSize, std::move(ws));

  // 1:1 with Java benchmark body:
  //   long sequence = ringBuffer.next();
  //   SimpleEvent e = ringBuffer.get(sequence);
  //   e.setValue(0);
  //   ringBuffer.publish(sequence);
  for (auto _ : state) {
    const int64_t sequence = rb->next();
    auto& e = rb->get(sequence);
    e.value = 0;
    rb->publish(sequence);
  }

  const auto wait_entries_after =
      disruptor::sp_wrap_wait_entries().load(std::memory_order_relaxed);
  const auto wait_loops_after =
      disruptor::sp_wrap_wait_loops().load(std::memory_order_relaxed);

  const double entries = static_cast<double>(wait_entries_after - wait_entries_before);
  const double loops = static_cast<double>(wait_loops_after - wait_loops_before);
  const double ops = static_cast<double>(state.iterations());

  // Raw totals (per repetition) for debugging.
  state.counters["sp_wrap_wait_entries"] = benchmark::Counter(entries, benchmark::Counter::kAvgThreads);
  state.counters["sp_wrap_wait_loops"] = benchmark::Counter(loops, benchmark::Counter::kAvgThreads);

  // Per-operation rates (more interpretable than raw counts).
  if (ops > 0) {
    state.counters["sp_wrap_wait_entries_per_op"] =
        benchmark::Counter(entries / ops, benchmark::Counter::kAvgThreads);
    state.counters["sp_wrap_wait_loops_per_op"] =
        benchmark::Counter(loops / ops, benchmark::Counter::kAvgThreads);
  }

}

static auto* bm_JMH_SingleProducerSingleConsumer_producing = [] {
  auto* b = benchmark::RegisterBenchmark("JMH_SingleProducerSingleConsumer_producing",
                                         &JMH_SingleProducerSingleConsumer_producing);
  return nano_stream::bench::jmh::applyJmhDefaults(b);
}();


