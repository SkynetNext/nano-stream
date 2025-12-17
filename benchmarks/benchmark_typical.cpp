#include "disruptor/ring_buffer.h"
#include "disruptor/sequence.h"
#include <atomic>
#include <benchmark/benchmark.h>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <thread>

using namespace disruptor;

struct TypicalEvent {
  int64_t value = 0;
  int64_t timestamp = 0;
  char padding[48]; // pad to cache line size
};

namespace {
constexpr size_t DISRUPTOR_RINGBUFFER_SIZE = 1u << 20; // reference/disruptor Constants.RINGBUFFER_SIZE
constexpr size_t DISRUPTOR_BIG_BUFFER_SIZE = 1u << 22; // reference/disruptor MultiProducerSingleConsumer.BIG_BUFFER
constexpr int DISRUPTOR_BATCH_SIZE = 100;              // reference/disruptor MultiProducerSingleConsumer.BATCH_SIZE

struct Mp1cSharedState {
  RingBuffer<TypicalEvent> ring_buffer;
  Sequence consumer_sequence;

  std::atomic<int64_t> produced{0};
  std::atomic<int64_t> consumed{0};
  std::atomic<int> producers_done{0};
  std::atomic<bool> stop{false};

  std::atomic<int> init_done{0};
  std::atomic<int> start_count{0};

  explicit Mp1cSharedState(size_t buffer_size)
      : ring_buffer(buffer_size, []() { return TypicalEvent(); }) {
    ring_buffer.add_gating_sequences({std::cref(consumer_sequence)});
  }

  void reset() {
    consumer_sequence.set(-1);
    produced.store(0, std::memory_order_relaxed);
    consumed.store(0, std::memory_order_relaxed);
    producers_done.store(0, std::memory_order_relaxed);
    stop.store(false, std::memory_order_relaxed);
    start_count.store(0, std::memory_order_relaxed);
    init_done.store(1, std::memory_order_release);
  }
};
} // namespace

// Typical production-style scenarios aligned to Disruptor JMH.

// 1) SingleProducerSingleConsumer:
//    - Disruptor: 1 producer benchmark thread; consumer runs in Disruptor-managed thread.
//    - Here: 1 producer (benchmark thread); 1 consumer background thread.
static void Typical_SingleProducerSingleConsumer(benchmark::State &state) {
  RingBuffer<TypicalEvent> ring_buffer(DISRUPTOR_RINGBUFFER_SIZE,
                                       []() { return TypicalEvent(); });

  Sequence consumer_sequence;
  ring_buffer.add_gating_sequences({std::cref(consumer_sequence)});

  // IMPORTANT: For strict comparability with Disruptor's JMH benchmark, we measure
  // ONLY the producer calls performed inside the benchmark loop. The consumer runs
  // in a background thread and is excluded from the measured "operations".
  std::atomic<bool> stop{false};
  std::atomic<int64_t> produced{0};
  std::atomic<int64_t> consumed{0};

  std::thread consumer([&]() {
    int64_t next_to_read = 0;
    while (true) {
      if (ring_buffer.is_available(next_to_read)) {
        const TypicalEvent &event = ring_buffer.get(next_to_read);
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
    TypicalEvent &event = ring_buffer.get(sequence);
    event.value = sequence;
    ring_buffer.publish(sequence);
    produced.fetch_add(1, std::memory_order_relaxed);
  }

  stop.store(true, std::memory_order_release);
  consumer.join();
  state.SetItemsProcessed(produced.load(std::memory_order_acquire));
}
BENCHMARK(Typical_SingleProducerSingleConsumer)->MinTime(1.0);

// 2) MultiProducerSingleConsumer (4 producers) single publish:
//    - Disruptor JMH uses @Threads(4) on producing().
//    - IMPORTANT: For strict comparability, we also measure ONLY producers in the benchmark threads.
//      The consumer runs as a background thread and is excluded from the measured "operations".
static void Typical_MultiProducerSingleConsumer(benchmark::State &state) {
  static Mp1cSharedState shared(DISRUPTOR_BIG_BUFFER_SIZE);
  const int threads = static_cast<int>(state.threads());
  const int producer_threads = threads;

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

  // Start consumer (background, not a benchmark thread), once.
  std::thread consumer;
  if (state.thread_index() == 0) {
    consumer = std::thread([&]() {
      int64_t next_to_read = 0;
      while (true) {
        if (shared.ring_buffer.is_available(next_to_read)) {
          const TypicalEvent &event = shared.ring_buffer.get(next_to_read);
          benchmark::DoNotOptimize(&event.value);
          shared.consumer_sequence.set(next_to_read);
          next_to_read++;
          shared.consumed.fetch_add(1, std::memory_order_relaxed);
        } else {
          if (shared.stop.load(std::memory_order_acquire) &&
              shared.consumed.load(std::memory_order_relaxed) >=
                  shared.produced.load(std::memory_order_acquire)) {
            break;
          }
          std::this_thread::yield();
        }
      }
    });
  }

  for (auto _ : state) {
    int64_t sequence = shared.ring_buffer.next();
    TypicalEvent &event = shared.ring_buffer.get(sequence);
    event.value = sequence;
    shared.ring_buffer.publish(sequence);
    shared.produced.fetch_add(1, std::memory_order_relaxed);
  }

  // Signal completion; only thread 0 is responsible for finalizing and joining.
  shared.producers_done.fetch_add(1, std::memory_order_release);
  if (state.thread_index() == 0) {
    // Wait until all producer threads are done, then stop consumer once all produced are consumed.
    while (shared.producers_done.load(std::memory_order_acquire) < producer_threads) {
      std::this_thread::yield();
    }
    shared.stop.store(true, std::memory_order_release);
    consumer.join();
    state.SetItemsProcessed(shared.produced.load(std::memory_order_acquire));
  }
}
BENCHMARK(Typical_MultiProducerSingleConsumer)->Threads(4)->MinTime(1.0);

// 3) MultiProducerSingleConsumer (4 producers) batch publish (batch=100):
static void Typical_MultiProducerSingleConsumerBatch(benchmark::State &state) {
  static Mp1cSharedState shared(DISRUPTOR_BIG_BUFFER_SIZE);
  const int threads = static_cast<int>(state.threads());
  const int producer_threads = threads;

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

  std::thread consumer;
  if (state.thread_index() == 0) {
    consumer = std::thread([&]() {
      int64_t next_to_read = 0;
      while (true) {
        if (shared.ring_buffer.is_available(next_to_read)) {
          const TypicalEvent &event = shared.ring_buffer.get(next_to_read);
          benchmark::DoNotOptimize(&event.value);
          shared.consumer_sequence.set(next_to_read);
          next_to_read++;
          shared.consumed.fetch_add(1, std::memory_order_relaxed);
        } else {
          if (shared.stop.load(std::memory_order_acquire) &&
              shared.consumed.load(std::memory_order_relaxed) >=
                  shared.produced.load(std::memory_order_acquire)) {
            break;
          }
          std::this_thread::yield();
        }
      }
    });
  }

  for (auto _ : state) {
    int64_t hi = shared.ring_buffer.next(DISRUPTOR_BATCH_SIZE);
    int64_t lo = hi - (DISRUPTOR_BATCH_SIZE - 1);
    for (int64_t seq = lo; seq <= hi; ++seq) {
      TypicalEvent &event = shared.ring_buffer.get(seq);
      event.value = seq;
    }
    // Our RingBuffer currently exposes publish(highestSequence) for batch; keep it consistent with existing usage.
    shared.ring_buffer.publish(hi);
    shared.produced.fetch_add(DISRUPTOR_BATCH_SIZE, std::memory_order_relaxed);
  }

  shared.producers_done.fetch_add(1, std::memory_order_release);
  if (state.thread_index() == 0) {
    while (shared.producers_done.load(std::memory_order_acquire) < producer_threads) {
      std::this_thread::yield();
    }
    shared.stop.store(true, std::memory_order_release);
    consumer.join();
    state.SetItemsProcessed(shared.produced.load(std::memory_order_acquire));
  }
}
BENCHMARK(Typical_MultiProducerSingleConsumerBatch)->Threads(4)->MinTime(1.0);


