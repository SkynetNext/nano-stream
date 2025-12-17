#include <benchmark/benchmark.h>

#include "jmh_config.h"
#include "jmh_util.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <thread>

namespace {
// Java: reference/disruptor/src/jmh/java/com/lmax/disruptor/util/Constants.java
constexpr int kRingBufferSize = 1 << 20;

class BoundedQueue {
public:
  explicit BoundedQueue(size_t capacity) : capacity_(capacity) {}

  bool offer(const nano_stream::bench::jmh::SimpleEvent& e, std::chrono::nanoseconds timeout) {
    std::unique_lock<std::mutex> lk(mu_);
    if (!cv_not_full_.wait_for(lk, timeout, [&] { return q_.size() < capacity_ || !running_; })) {
      return false;
    }
    if (!running_) return false;
    q_.push(e);
    cv_not_empty_.notify_one();
    return true;
  }

  bool poll(nano_stream::bench::jmh::SimpleEvent& out) {
    std::lock_guard<std::mutex> lk(mu_);
    if (q_.empty()) return false;
    out = q_.front();
    q_.pop();
    cv_not_full_.notify_one();
    return true;
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
  std::queue<nano_stream::bench::jmh::SimpleEvent> q_;
  size_t capacity_;
  bool running_{true};
};
} // namespace

// 1:1-ish baseline with Java:
// reference/disruptor/src/jmh/java/com/lmax/disruptor/BlockingQueueBenchmark.java
static void JMH_BlockingQueueBenchmark_producing(benchmark::State& state) {
  BoundedQueue q(static_cast<size_t>(kRingBufferSize));
  std::atomic<bool> consumerRunning{true};

  std::thread consumer([&] {
    nano_stream::bench::jmh::SimpleEvent tmp{};
    while (consumerRunning.load(std::memory_order_acquire)) {
      if (q.poll(tmp)) {
        benchmark::DoNotOptimize(tmp.value);
      } else {
        std::this_thread::yield();
      }
    }
  });

  nano_stream::bench::jmh::SimpleEvent e{};
  e.value = 0;

  for (auto _ : state) {
    if (!q.offer(e, std::chrono::seconds(1))) {
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
  return nano_stream::bench::jmh::applyJmhDefaults(b);
}();


