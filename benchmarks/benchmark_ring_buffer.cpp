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

namespace {
constexpr size_t DISRUPTOR_RINGBUFFER_SIZE = 1u << 20; // match reference/disruptor Constants.RINGBUFFER_SIZE
constexpr size_t DISRUPTOR_BIG_BUFFER_SIZE = 1u << 22; // match MultiProducerSingleConsumer.BIG_BUFFER
constexpr int DISRUPTOR_BATCH_SIZE = 100;              // match MultiProducerSingleConsumer.BATCH_SIZE

struct Mp1cSharedState {
  RingBuffer<BenchmarkEvent> ring_buffer;
  Sequence consumer_sequence;

  std::atomic<int64_t> produced{0};
  std::atomic<int64_t> consumed{0};
  std::atomic<int> producers_done{0};

  // Simple barriers so consumer/producers start together per benchmark run
  std::atomic<int> init_done{0};
  std::atomic<int> start_count{0};

  explicit Mp1cSharedState(size_t buffer_size)
      : ring_buffer(buffer_size, []() { return BenchmarkEvent(); }) {
    ring_buffer.add_gating_sequences({std::cref(consumer_sequence)});
  }

  void reset() {
    consumer_sequence.set(-1);
    produced.store(0, std::memory_order_relaxed);
    consumed.store(0, std::memory_order_relaxed);
    producers_done.store(0, std::memory_order_relaxed);
    start_count.store(0, std::memory_order_relaxed);
    init_done.store(1, std::memory_order_release);
  }
};
} // namespace

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

// Typical production-style scenarios aligned to Disruptor JMH:
//
// 1) SingleProducerSingleConsumer:
//    - Disruptor: 1 producer benchmark thread; consumer runs in Disruptor-managed thread.
//    - Here: 1 producer (benchmark thread); 1 consumer background thread.
//    - Ring size: 1<<20.
static void BM_Typical_SingleProducerSingleConsumer(benchmark::State &state) {
  RingBuffer<BenchmarkEvent> ring_buffer(DISRUPTOR_RINGBUFFER_SIZE,
                                         []() { return BenchmarkEvent(); });

  Sequence consumer_sequence;
  ring_buffer.add_gating_sequences({std::cref(consumer_sequence)});

  std::atomic<bool> stop{false};
  std::atomic<int64_t> produced{0};
  std::atomic<int64_t> consumed{0};

  std::thread consumer([&]() {
    int64_t next_to_read = 0;
    while (true) {
      if (ring_buffer.is_available(next_to_read)) {
        const BenchmarkEvent &event = ring_buffer.get(next_to_read);
        benchmark::DoNotOptimize(&event.value);
        consumer_sequence.set(next_to_read);
        next_to_read++;
        consumed.fetch_add(1, std::memory_order_relaxed);
      } else {
        if (stop.load(std::memory_order_acquire) &&
            consumed.load(std::memory_order_relaxed) >=
                produced.load(std::memory_order_acquire)) {
          break;
        }
        std::this_thread::yield();
      }
    }
  });

  for (auto _ : state) {
    int64_t sequence = ring_buffer.next();
    BenchmarkEvent &event = ring_buffer.get(sequence);
    event.value = sequence;
    ring_buffer.publish(sequence);
    produced.fetch_add(1, std::memory_order_relaxed);
  }

  stop.store(true, std::memory_order_release);
  consumer.join();
  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Typical_SingleProducerSingleConsumer)->MinTime(1.0);

// 2) MultiProducerSingleConsumer (4 producers) single publish:
//    - Disruptor JMH uses @Threads(4) on producing().
//    - Here we model 4 producer benchmark threads + 1 consumer benchmark thread (total 5).
static void BM_Typical_MultiProducerSingleConsumer(benchmark::State &state) {
  // Shared state across benchmark threads, constructed once per process.
  static Mp1cSharedState shared(DISRUPTOR_BIG_BUFFER_SIZE);
  const int threads = static_cast<int>(state.threads());
  const int producer_threads = threads - 1;

  if (state.thread_index() == 0) {
    shared.init_done.store(0, std::memory_order_relaxed);
    shared.reset();
  }
  while (shared.init_done.load(std::memory_order_acquire) == 0) {
    // wait for init
  }
  shared.start_count.fetch_add(1, std::memory_order_acq_rel);
  while (shared.start_count.load(std::memory_order_acquire) < threads) {
    // barrier
  }

  if (state.thread_index() == 0) {
    // Consumer thread
    int64_t next_to_read = 0;
    while (true) {
      if (shared.ring_buffer.is_available(next_to_read)) {
        const BenchmarkEvent &event = shared.ring_buffer.get(next_to_read);
        benchmark::DoNotOptimize(&event.value);
        shared.consumer_sequence.set(next_to_read);
        next_to_read++;
        shared.consumed.fetch_add(1, std::memory_order_relaxed);
      } else {
        if (shared.producers_done.load(std::memory_order_acquire) ==
                producer_threads &&
            shared.consumed.load(std::memory_order_relaxed) >=
                shared.produced.load(std::memory_order_acquire)) {
          break;
        }
        std::this_thread::yield();
      }
    }

    state.SetItemsProcessed(state.iterations() * producer_threads);
  } else {
    // Producer threads
    for (auto _ : state) {
      int64_t sequence = shared.ring_buffer.next();
      BenchmarkEvent &event = shared.ring_buffer.get(sequence);
      event.value = sequence;
      shared.ring_buffer.publish(sequence);
      shared.produced.fetch_add(1, std::memory_order_relaxed);
    }
    shared.producers_done.fetch_add(1, std::memory_order_release);
  }
}
BENCHMARK(BM_Typical_MultiProducerSingleConsumer)->Threads(5)->MinTime(1.0);

// 3) MultiProducerSingleConsumer (4 producers) batch publish (batch=100):
static void BM_Typical_MultiProducerSingleConsumerBatch(benchmark::State &state) {
  static Mp1cSharedState shared(DISRUPTOR_BIG_BUFFER_SIZE);
  const int threads = static_cast<int>(state.threads());
  const int producer_threads = threads - 1;

  if (state.thread_index() == 0) {
    shared.init_done.store(0, std::memory_order_relaxed);
    shared.reset();
  }
  while (shared.init_done.load(std::memory_order_acquire) == 0) {
    // wait for init
  }
  shared.start_count.fetch_add(1, std::memory_order_acq_rel);
  while (shared.start_count.load(std::memory_order_acquire) < threads) {
    // barrier
  }

  if (state.thread_index() == 0) {
    int64_t next_to_read = 0;
    while (true) {
      if (shared.ring_buffer.is_available(next_to_read)) {
        const BenchmarkEvent &event = shared.ring_buffer.get(next_to_read);
        benchmark::DoNotOptimize(&event.value);
        shared.consumer_sequence.set(next_to_read);
        next_to_read++;
        shared.consumed.fetch_add(1, std::memory_order_relaxed);
      } else {
        if (shared.producers_done.load(std::memory_order_acquire) ==
                producer_threads &&
            shared.consumed.load(std::memory_order_relaxed) >=
                shared.produced.load(std::memory_order_acquire)) {
          break;
        }
        std::this_thread::yield();
      }
    }

    state.SetItemsProcessed(state.iterations() * producer_threads *
                            DISRUPTOR_BATCH_SIZE);
  } else {
    for (auto _ : state) {
      int64_t hi = shared.ring_buffer.next(DISRUPTOR_BATCH_SIZE);
      int64_t lo = hi - (DISRUPTOR_BATCH_SIZE - 1);
      for (int64_t seq = lo; seq <= hi; ++seq) {
        BenchmarkEvent &event = shared.ring_buffer.get(seq);
        event.value = seq;
      }
      shared.ring_buffer.publish(hi);
      shared.produced.fetch_add(DISRUPTOR_BATCH_SIZE, std::memory_order_relaxed);
    }
    shared.producers_done.fetch_add(1, std::memory_order_release);
  }
}
BENCHMARK(BM_Typical_MultiProducerSingleConsumerBatch)->Threads(5)->MinTime(1.0);

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
