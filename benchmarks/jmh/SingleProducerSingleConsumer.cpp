#include <benchmark/benchmark.h>

#include "jmh_config.h"
#include "jmh_util.h"

#include "disruptor/BusySpinWaitStrategy.h"
#include "disruptor/dsl/Disruptor.h"
#include "disruptor/dsl/ProducerType.h"
#include "disruptor/util/DaemonThreadFactory.h"

#include <memory>

namespace {
// Java: reference/disruptor/src/jmh/java/com/lmax/disruptor/util/Constants.java
constexpr int kRingBufferSize = 1 << 20;
} // namespace

// 1:1 with Java JMH class:
// reference/disruptor/src/jmh/java/com/lmax/disruptor/SingleProducerSingleConsumer.java
static void JMH_SingleProducerSingleConsumer_producing(benchmark::State& state) {
  auto& tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  auto ws = std::make_shared<disruptor::BusySpinWaitStrategy>();
  auto factory = std::make_shared<nano_stream::bench::jmh::SimpleEventFactory>();

  disruptor::dsl::Disruptor<nano_stream::bench::jmh::SimpleEvent> d(
      factory, kRingBufferSize, tf, disruptor::dsl::ProducerType::SINGLE, ws);
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

static auto* bm_JMH_SingleProducerSingleConsumer_producing = [] {
  auto* b = benchmark::RegisterBenchmark("JMH_SingleProducerSingleConsumer_producing",
                                         &JMH_SingleProducerSingleConsumer_producing);
  return nano_stream::bench::jmh::applyJmhDefaults(b);
}();


