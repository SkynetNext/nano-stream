#include "nano_stream/ring_buffer.h"
#include "nano_stream/sequence.h"
#include <atomic>
#include <benchmark/benchmark.h>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>

using namespace nano_stream;

struct TestEvent {
  int64_t value = 0;
  int64_t timestamp = 0;
};

// Thread-safe queue for comparison
template <typename T> class ThreadSafeQueue {
private:
  mutable std::mutex mutex_;
  std::queue<T> queue_;
  std::condition_variable condition_;

public:
  void push(const T &item) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push(item);
    condition_.notify_one();
  }

  bool try_pop(T &item) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
      return false;
    }
    item = queue_.front();
    queue_.pop();
    return true;
  }

  bool wait_and_pop(T &item) {
    std::unique_lock<std::mutex> lock(mutex_);
    while (queue_.empty()) {
      condition_.wait(lock);
    }
    item = queue_.front();
    queue_.pop();
    return true;
  }

  bool empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
  }

  size_t size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }
};

// Benchmark nano-stream RingBuffer
static void BM_NanoStreamProducerConsumer(benchmark::State &state) {
  const int num_events = state.range(0);
  RingBuffer<TestEvent> ring_buffer(16384, []() { return TestEvent(); });

  Sequence consumer_sequence;
  ring_buffer.add_gating_sequences({std::cref(consumer_sequence)});

  std::atomic<bool> stop{false};
  std::atomic<int> total_produced{0};
  std::atomic<int> total_consumed{0};

  std::thread consumer([&]() {
    int64_t next_to_read = 0;

    while (true) {
      if (stop.load(std::memory_order_acquire)) {
        int produced = total_produced.load(std::memory_order_acquire);
        if (total_consumed.load(std::memory_order_relaxed) >= produced) {
          break;
        }
      }

      if (ring_buffer.is_available(next_to_read)) {
        const TestEvent &event = ring_buffer.get(next_to_read);
        benchmark::DoNotOptimize(&event.value);
        consumer_sequence.set(next_to_read);
        next_to_read++;
        total_consumed.fetch_add(1, std::memory_order_relaxed);
      } else {
        std::this_thread::yield();
      }
    }
  });

  for (auto _ : state) {
    for (int i = 0; i < num_events; ++i) {
      int64_t sequence = ring_buffer.next();
      TestEvent &event = ring_buffer.get(sequence);
      event.value = i;
      ring_buffer.publish(sequence);
      total_produced.fetch_add(1, std::memory_order_relaxed);
    }
  }

  // Signal stop and wait for all items to be consumed
  stop.store(true, std::memory_order_release);
  consumer.join();

  state.SetItemsProcessed(state.iterations() * num_events);
}
BENCHMARK(BM_NanoStreamProducerConsumer)->Arg(1000)->Arg(10000);

// Benchmark std::queue with mutex
static void BM_StdQueueProducerConsumer(benchmark::State &state) {
  const int num_events = state.range(0);
  ThreadSafeQueue<TestEvent> queue;

  std::atomic<bool> stop{false};
  std::atomic<int> total_produced{0};
  std::atomic<int> total_consumed{0};

  std::thread consumer([&]() {
    TestEvent event;

    while (true) {
      if (stop.load(std::memory_order_acquire)) {
        int produced = total_produced.load(std::memory_order_acquire);
        if (total_consumed.load(std::memory_order_relaxed) >= produced) {
          break;
        }
      }

      if (queue.try_pop(event)) {
        benchmark::DoNotOptimize(&event.value);
        total_consumed.fetch_add(1, std::memory_order_relaxed);
      } else {
        std::this_thread::yield();
      }
    }
  });

  for (auto _ : state) {
    for (int i = 0; i < num_events; ++i) {
      TestEvent event;
      event.value = i;
      queue.push(event);
      total_produced.fetch_add(1, std::memory_order_relaxed);
    }
  }

  // Signal stop and wait for all items to be consumed
  stop.store(true, std::memory_order_release);
  consumer.join();

  state.SetItemsProcessed(state.iterations() * num_events);
}
BENCHMARK(BM_StdQueueProducerConsumer)->Arg(1000)->Arg(10000);

// Single-threaded throughput comparison
static void BM_NanoStreamSingleThreaded(benchmark::State &state) {
  RingBuffer<TestEvent> ring_buffer(16384, []() { return TestEvent(); });
  int64_t counter = 0;

  for (auto _ : state) {
    int64_t sequence = ring_buffer.next();
    TestEvent &event = ring_buffer.get(sequence);
    event.value = counter++;
    ring_buffer.publish(sequence);
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_NanoStreamSingleThreaded);

static void BM_StdQueueSingleThreaded(benchmark::State &state) {
  std::queue<TestEvent> queue;
  int64_t counter = 0;

  for (auto _ : state) {
    TestEvent event;
    event.value = counter++;
    queue.push(event);

    // Also pop to keep queue size manageable
    if (!queue.empty()) {
      TestEvent popped = queue.front();
      queue.pop();
      benchmark::DoNotOptimize(&popped.value);
    }
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_StdQueueSingleThreaded);

// Latency measurement (using try_next for low-latency scenarios)
static void BM_NanoStreamLowLatency(benchmark::State &state) {
  RingBuffer<TestEvent> ring_buffer(16384, []() { return TestEvent(); });

  for (auto _ : state) {
    try {
      int64_t sequence = ring_buffer.try_next();
      TestEvent &event = ring_buffer.get(sequence);
      event.value = 42;
      ring_buffer.publish(sequence);
    } catch (const InsufficientCapacityException &) {
      // Expected when buffer is full
    }
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_NanoStreamLowLatency);

// Memory usage comparison
static void BM_NanoStreamMemoryAccess(benchmark::State &state) {
  const size_t buffer_size = 16384;
  RingBuffer<TestEvent> ring_buffer(buffer_size, []() { return TestEvent(); });

  // Pre-populate
  for (size_t i = 0; i < buffer_size; ++i) {
    int64_t sequence = ring_buffer.next();
    TestEvent &event = ring_buffer.get(sequence);
    event.value = i;
    ring_buffer.publish(sequence);
  }

  size_t index = 0;
  for (auto _ : state) {
    const TestEvent &event = ring_buffer.get(index);
    benchmark::DoNotOptimize(&event.value);
    index = (index + 1) % buffer_size;
  }

  state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_NanoStreamMemoryAccess);
