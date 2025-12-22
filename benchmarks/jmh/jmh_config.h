#pragma once
// Shared JMH-like execution model settings for C++ Google Benchmark ports.
//
// Mirrors CI defaults used for Java JMH:
//   -f 1 -wi 3 -i 5
//
// NOTE: This is an approximation: JMH is time-iteration + forked JVM; in C++ we
// approximate warmup by using Google Benchmark's built-in warmup support.

#include <benchmark/benchmark.h>

#include <chrono>

namespace disruptor::bench::jmh {

inline benchmark::internal::Benchmark* applyJmhDefaults(benchmark::internal::Benchmark* b) {
  // Keep the benchmark registration free of timing/repetition policy so that:
  // - Local runs can quickly override via CLI flags
  // - CI can enforce strict JMH-like policy via CLI flags
  return b->ReportAggregatesOnly(true);
}

} // namespace disruptor::bench::jmh


