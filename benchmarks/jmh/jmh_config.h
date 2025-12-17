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

namespace nano_stream::bench::jmh {

constexpr int kMeasurementRepetitions = 5;
// Match Java JMH defaults used in CI:
//   Warmup:      -wi 3  (3 iterations)
//   Warmup time: 10 s each  => 30 s total
//   Measure:     -i 5   (5 iterations)
//   Measure time: 10 s each => use MinTime(10s) with Repetitions(5)
//
// NOTE: Google Benchmark's warmup is a single duration, not iteration-based.
constexpr double kMinWarmupTimeSeconds = 30.0;
constexpr double kMinTimeSeconds = 10.0;

inline benchmark::internal::Benchmark* applyJmhDefaults(benchmark::internal::Benchmark* b) {
  return b->MinWarmUpTime(kMinWarmupTimeSeconds)
      ->MinTime(kMinTimeSeconds)
      ->Repetitions(kMeasurementRepetitions)
      ->ReportAggregatesOnly(true);
}

} // namespace nano_stream::bench::jmh


