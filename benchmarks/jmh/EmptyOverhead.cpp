#include <benchmark/benchmark.h>

#include "jmh_config.h"

#include <cstdint>

// Measure Google Benchmark + loop overhead baseline for extremely small ns/op benchmarks.
static void JMH_EmptyLoop_DoNotOptimize(benchmark::State& state) {
  uint64_t x = 0;
  for (auto _ : state) {
    benchmark::DoNotOptimize(x);
    x++;
  }
}

static auto* bm_JMH_EmptyLoop_DoNotOptimize = [] {
  auto* b = benchmark::RegisterBenchmark("JMH_EmptyLoop_DoNotOptimize", &JMH_EmptyLoop_DoNotOptimize);
  return disruptor::bench::jmh::applyJmhDefaults(b);
}();


