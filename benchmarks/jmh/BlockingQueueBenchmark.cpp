#include <benchmark/benchmark.h>

#include "jmh_config.h"
#include "jmh_util.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

namespace {
// Java: reference/disruptor/src/jmh/java/com/lmax/disruptor/util/Constants.java
constexpr int kRingBufferSize = 1 << 20;

// Java baseline uses ArrayBlockingQueue (fixed-size array + single lock).
// We implement the same structure (bounded circular buffer) and expose:
// - offer(e, timeout)
// - poll() (non-blocking)
//
// IMPORTANT: Java benchmark repeatedly enqueues the SAME SimpleEvent instance; we mirror by storing pointers.
class ArrayBlockingQueueLike {
public:
  explicit ArrayBlockingQueueLike(size_t capacity)
      : capacity_(capacity), buf_(capacity_, nullptr) {}

  bool offer(disruptor::bench::jmh::SimpleEvent* e, std::chrono::nanoseconds timeout) {
    std::unique_lock<std::mutex> lk(mu_);

    // Fast path: do not call into condition_variable if we are not full.
    // Java's ArrayBlockingQueue.offer(timeout) only waits when full.
    if (count_ >= capacity_) {
      if (!cv_not_full_.wait_for(lk, timeout, [&] { return count_ < capacity_ || !running_; })) {
        return false;
      }
      if (!running_) return false;
    }
    const bool was_empty = (count_ == 0);
    buf_[tail_] = e;
    tail_ = (tail_ + 1) % capacity_;
    ++count_;
    // Java signals notEmpty only on transitions from empty -> non-empty.
    if (was_empty) {
      cv_not_empty_.notify_one();
    }
    return true;
  }

  disruptor::bench::jmh::SimpleEvent* poll() {
    std::lock_guard<std::mutex> lk(mu_);
    if (count_ == 0) return nullptr;
    const bool was_full = (count_ == capacity_);
    auto* out = buf_[head_];
    buf_[head_] = nullptr;
    head_ = (head_ + 1) % capacity_;
    --count_;
    // Java signals notFull only on transitions from full -> not-full.
    if (was_full) {
      cv_not_full_.notify_one();
    }
    return out;
  }

  void stop() {
    {
      std::lock_guard<std::mutex> lk(mu_);
      running_ = false;
    }
    cv_not_empty_.notify_all();
    cv_not_full_.notify_all();
  }

private:
  std::mutex mu_;
  std::condition_variable cv_not_empty_;
  std::condition_variable cv_not_full_;
  size_t capacity_{0};
  std::vector<disruptor::bench::jmh::SimpleEvent*> buf_;
  size_t head_{0};
  size_t tail_{0};
  size_t count_{0};
  bool running_{true};
};
} // namespace

// 1:1 with Java:
// reference/disruptor/src/jmh/java/com/lmax/disruptor/BlockingQueueBenchmark.java
static void JMH_BlockingQueueBenchmark_producing(benchmark::State& state) {
  ArrayBlockingQueueLike q(static_cast<size_t>(kRingBufferSize));
  std::atomic<bool> consumerRunning{true}; // mirrors Java volatile boolean
  std::promise<void> started;
  auto started_f = started.get_future();

  std::thread consumer([&] {
    started.set_value();
    while (consumerRunning.load(std::memory_order_acquire)) {
      auto* ev = q.poll();
      if (ev != nullptr) benchmark::DoNotOptimize(ev->value);
    }
  });

  // Wait for consumer thread to start (CountDownLatch analogue).
  started_f.wait();

  // Java reuses the same SimpleEvent instance for all offers.
  disruptor::bench::jmh::SimpleEvent e{};
  e.value = 0;

  for (auto _ : state) {
    if (!q.offer(&e, std::chrono::seconds(1))) {
      state.SkipWithError("Queue full, benchmark should not experience backpressure");
      break;
    }
  }

  consumerRunning.store(false, std::memory_order_release);
  q.stop();
  consumer.join();
}

static auto* bm_JMH_BlockingQueueBenchmark_producing = [] {
  auto* b = benchmark::RegisterBenchmark("JMH_BlockingQueueBenchmark_producing",
                                         &JMH_BlockingQueueBenchmark_producing);
  return disruptor::bench::jmh::applyJmhDefaults(b);
}();


