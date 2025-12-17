#include <benchmark/benchmark.h>

#include "jmh_config.h"
#include "jmh_util.h"

#include "disruptor/BusySpinWaitStrategy.h"
#include "disruptor/dsl/Disruptor.h"
#include "disruptor/dsl/ProducerType.h"
#include "disruptor/util/DaemonThreadFactory.h"

#include <cstdint>
#include <memory>

namespace {
// Java: reference/disruptor/src/jmh/java/com/lmax/disruptor/MultiProducerSingleConsumer.java
constexpr int kBigBuffer = 1 << 22;
constexpr int kBatchSize = 100;
constexpr int kMpThreads = 4;
} // namespace

// 1:1 with Java JMH class:
// reference/disruptor/src/jmh/java/com/lmax/disruptor/MultiProducerSingleConsumer.java
static void JMH_MultiProducerSingleConsumer_producing(benchmark::State& state) {
  auto& tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  auto ws = std::make_unique<disruptor::BusySpinWaitStrategy>();
  auto factory = std::make_shared<nano_stream::bench::jmh::SimpleEventFactory>();

  disruptor::dsl::Disruptor<nano_stream::bench::jmh::SimpleEvent> d(
      factory, kBigBuffer, tf, disruptor::dsl::ProducerType::MULTI, std::move(ws));
  nano_stream::bench::jmh::ConsumeHandler handler;
  d.handleEventsWith(handler);
  auto rb = d.start();

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
  d.shutdown();
}

static auto* bm_JMH_MultiProducerSingleConsumer_producing = [] {
  auto* b = benchmark::RegisterBenchmark("JMH_MultiProducerSingleConsumer_producing",
                                         &JMH_MultiProducerSingleConsumer_producing);
  b->Threads(kMpThreads);
  return nano_stream::bench::jmh::applyJmhDefaults(b);
}();

// 1:1 with Java JMH method producingBatch(), including OperationsPerInvocation(BATCH_SIZE).
static void JMH_MultiProducerSingleConsumer_producingBatch(benchmark::State& state) {
  auto& tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  auto ws = std::make_unique<disruptor::BusySpinWaitStrategy>();
  auto factory = std::make_shared<nano_stream::bench::jmh::SimpleEventFactory>();

  disruptor::dsl::Disruptor<nano_stream::bench::jmh::SimpleEvent> d(
      factory, kBigBuffer, tf, disruptor::dsl::ProducerType::MULTI, std::move(ws));
  nano_stream::bench::jmh::ConsumeHandler handler;
  d.handleEventsWith(handler);
  auto rb = d.start();

  for (auto _ : state) {
    int64_t hi = rb->next(kBatchSize);
    int64_t lo = hi - (kBatchSize - 1);
    for (int64_t seq = lo; seq <= hi; ++seq) {
      auto& e = rb->get(seq);
      e.value = 0;
    }
    rb->publish(lo, hi);
  }

  // JMH: @OperationsPerInvocation(BATCH_SIZE)
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kBatchSize));

  d.shutdown();
}

static auto* bm_JMH_MultiProducerSingleConsumer_producingBatch = [] {
  auto* b = benchmark::RegisterBenchmark("JMH_MultiProducerSingleConsumer_producingBatch",
                                         &JMH_MultiProducerSingleConsumer_producingBatch);
  b->Threads(kMpThreads);
  return nano_stream::bench::jmh::applyJmhDefaults(b);
}();


