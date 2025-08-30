#include "disruptor/sequence.h"
#include <atomic>
#include <benchmark/benchmark.h>
#include <cstdint>
#include <thread>

using namespace disruptor;

// Benchmark basic sequence operations
static void BM_SequenceGet(benchmark::State &state) {
  Sequence seq(100);

  for (auto _ : state) {
    int64_t value = seq.get();
    benchmark::DoNotOptimize(&value);
  }
}
BENCHMARK(BM_SequenceGet);

static void BM_SequenceSet(benchmark::State &state) {
  Sequence seq;
  int64_t counter = 0;

  for (auto _ : state) {
    seq.set(counter++);
  }
}
BENCHMARK(BM_SequenceSet);

static void BM_SequenceIncrementAndGet(benchmark::State &state) {
  Sequence seq(0);

  for (auto _ : state) {
    int64_t value = seq.increment_and_get();
    benchmark::DoNotOptimize(&value);
  }
}
BENCHMARK(BM_SequenceIncrementAndGet);

static void BM_SequenceCompareAndSet(benchmark::State &state) {
  Sequence seq(0);
  int64_t expected = 0;

  for (auto _ : state) {
    if (seq.compare_and_set(expected, expected + 1)) {
      expected++;
    }
  }
}
BENCHMARK(BM_SequenceCompareAndSet);

// Compare with std::atomic
static void BM_AtomicLoad(benchmark::State &state) {
  std::atomic<int64_t> atomic_val{100};

  for (auto _ : state) {
    int64_t value = atomic_val.load(std::memory_order_acquire);
    benchmark::DoNotOptimize(&value);
  }
}
BENCHMARK(BM_AtomicLoad);

static void BM_AtomicStore(benchmark::State &state) {
  std::atomic<int64_t> atomic_val{0};
  int64_t counter = 0;

  for (auto _ : state) {
    atomic_val.store(counter++, std::memory_order_release);
  }
}
BENCHMARK(BM_AtomicStore);

static void BM_AtomicFetchAdd(benchmark::State &state) {
  std::atomic<int64_t> atomic_val{0};

  for (auto _ : state) {
    int64_t value = atomic_val.fetch_add(1, std::memory_order_acq_rel);
    benchmark::DoNotOptimize(&value);
  }
}
BENCHMARK(BM_AtomicFetchAdd);

// Concurrent access benchmarks
static void BM_SequenceConcurrentIncrements(benchmark::State &state) {
  static Sequence seq(0);

  for (auto _ : state) {
    seq.increment_and_get();
  }
}
BENCHMARK(BM_SequenceConcurrentIncrements)
    ->ThreadRange(1, std::thread::hardware_concurrency());

static void BM_AtomicConcurrentIncrements(benchmark::State &state) {
  static std::atomic<int64_t> atomic_val{0};

  for (auto _ : state) {
    atomic_val.fetch_add(1, std::memory_order_acq_rel);
  }
}
BENCHMARK(BM_AtomicConcurrentIncrements)
    ->ThreadRange(1, std::thread::hardware_concurrency());
