#include "disruptor/ring_buffer.h"
#include "disruptor/sequence.h"
#include <atomic>
#include <benchmark/benchmark.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <thread>
#include <vector>

using namespace disruptor;

struct BenchmarkEvent {
  int64_t value = 0;
  int64_t timestamp = 0;
  char padding[48]; // Pad to cache line size
};

// Single producer benchmarks
static void BM_RingBufferSingleProducer(benchmark::State &state) {
  const size_t buffer_size = state.range(0);
  auto factory = []() { return BenchmarkEvent(); };
  RingBuffer<BenchmarkEvent> ring_buffer =
      RingBuffer<BenchmarkEvent>::createSingleProducer(buffer_size, factory);

  int64_t counter = 0;

  // Warmup: perform iterations to warm up cache and branch predictors
  // For RingBuffer, we need to fill the buffer multiple times to ensure
  // the ring buffer wraps around and stabilizes
  const int warmup_iterations = buffer_size * 3; // Fill buffer 3 times
  for (int i = 0; i < warmup_iterations; ++i) {
    int64_t sequence = ring_buffer.next();
    BenchmarkEvent &event = ring_buffer.get(sequence);
    event.value = counter++;
    ring_buffer.publish(sequence);
  }

  // Reset counter for actual benchmark
  counter = 0;

  for (auto _ : state) {
    int64_t sequence = ring_buffer.next();
    BenchmarkEvent &event = ring_buffer.get(sequence);
    event.value = counter++;
    ring_buffer.publish(sequence);
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RingBufferSingleProducer)
    ->Arg(1024)
    ->Arg(4096)
    ->Arg(16384)
    ->MinTime(1.0); // Ensure at least 1 second of measurement time for accurate
                    // statistics

static void BM_RingBufferBatchProducer(benchmark::State &state) {
  const size_t buffer_size = 16384;
  const int batch_size = state.range(0);
  RingBuffer<BenchmarkEvent> ring_buffer(buffer_size,
                                         []() { return BenchmarkEvent(); });

  int64_t counter = 0;

  for (auto _ : state) {
    int64_t sequence = ring_buffer.next(batch_size);

    for (int i = 0; i < batch_size; ++i) {
      int64_t seq = sequence - batch_size + 1 + i;
      BenchmarkEvent &event = ring_buffer.get(seq);
      event.value = counter++;
    }

    ring_buffer.publish(sequence);
  }

  state.SetItemsProcessed(state.iterations() * batch_size);
}
BENCHMARK(BM_RingBufferBatchProducer)->Range(1, 64);

static void BM_RingBufferTryNext(benchmark::State &state) {
  const size_t buffer_size = 16384;
  RingBuffer<BenchmarkEvent> ring_buffer(buffer_size,
                                         []() { return BenchmarkEvent(); });

  int64_t counter = 0;

  for (auto _ : state) {
    try {
      int64_t sequence = ring_buffer.try_next();
      BenchmarkEvent &event = ring_buffer.get(sequence);
      event.value = counter++;
      ring_buffer.publish(sequence);
    } catch (const InsufficientCapacityException &) {
      // Expected when buffer is full
    }
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RingBufferTryNext);

// Producer-Consumer benchmarks
static void BM_RingBufferProducerConsumer(benchmark::State &state) {
  const size_t buffer_size = 16384;
  RingBuffer<BenchmarkEvent> ring_buffer(buffer_size,
                                         []() { return BenchmarkEvent(); });

  // Consumer sequence
  Sequence consumer_sequence;
  ring_buffer.add_gating_sequences({std::cref(consumer_sequence)});

  std::atomic<bool> stop{false};
  std::atomic<int64_t> produced_count{0};
  std::atomic<int64_t> consumed_count{0};

  // Consumer thread
  std::thread consumer([&]() {
    int64_t next_to_read = 0;

    while (true) {
      // Check if we should stop and have consumed all produced items
      if (stop.load(std::memory_order_acquire)) {
        int64_t total_produced = produced_count.load(std::memory_order_acquire);
        if (consumed_count.load(std::memory_order_relaxed) >= total_produced) {
          break;
        }
      }

      if (ring_buffer.is_available(next_to_read)) {
        const BenchmarkEvent &event = ring_buffer.get(next_to_read);
        benchmark::DoNotOptimize(&event.value);
        consumer_sequence.set(next_to_read);
        next_to_read++;
        consumed_count.fetch_add(1, std::memory_order_relaxed);
      } else {
        std::this_thread::yield();
      }
    }
  });

  // Producer (main thread)
  int64_t counter = 0;
  for (auto _ : state) {
    int64_t sequence = ring_buffer.next();
    BenchmarkEvent &event = ring_buffer.get(sequence);
    event.value = counter++;
    ring_buffer.publish(sequence);
    produced_count.fetch_add(1, std::memory_order_relaxed);
  }

  // Signal stop and wait for consumer to finish
  stop.store(true, std::memory_order_release);
  consumer.join();

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RingBufferProducerConsumer)
    ->MinTime(0.5)         // 最小运行 0.5 秒
    ->Iterations(1000000); // 或固定 100万次迭代

// Memory access patterns
static void BM_RingBufferSequentialAccess(benchmark::State &state) {
  const size_t buffer_size = 16384;
  RingBuffer<BenchmarkEvent> ring_buffer(buffer_size,
                                         []() { return BenchmarkEvent(); });

  // Pre-populate the buffer
  for (size_t i = 0; i < buffer_size; ++i) {
    int64_t sequence = ring_buffer.next();
    BenchmarkEvent &event = ring_buffer.get(sequence);
    event.value = i;
    ring_buffer.publish(sequence);
  }

  size_t index = 0;
  for (auto _ : state) {
    const BenchmarkEvent &event = ring_buffer.get(index);
    benchmark::DoNotOptimize(&event.value);
    index = (index + 1) % buffer_size;
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RingBufferSequentialAccess);

static void BM_RingBufferRandomAccess(benchmark::State &state) {
  const size_t buffer_size = 16384;
  RingBuffer<BenchmarkEvent> ring_buffer(buffer_size,
                                         []() { return BenchmarkEvent(); });

  // Pre-populate the buffer
  for (size_t i = 0; i < buffer_size; ++i) {
    int64_t sequence = ring_buffer.next();
    BenchmarkEvent &event = ring_buffer.get(sequence);
    event.value = i;
    ring_buffer.publish(sequence);
  }

  // Generate random indices
  std::vector<size_t> indices;
  for (size_t i = 0; i < 10000; ++i) {
    indices.push_back(std::hash<size_t>{}(i) % buffer_size);
  }

  size_t idx_counter = 0;
  for (auto _ : state) {
    size_t index = indices[idx_counter % indices.size()];
    const BenchmarkEvent &event = ring_buffer.get(index);
    benchmark::DoNotOptimize(&event.value);
    idx_counter++;
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RingBufferRandomAccess);
