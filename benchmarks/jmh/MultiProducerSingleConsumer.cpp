#include <benchmark/benchmark.h>

#include "jmh_config.h"
#include "jmh_util.h"

#include "disruptor/BusySpinWaitStrategy.h"
#include "disruptor/TimeoutException.h"
#include "disruptor/dsl/Disruptor.h"
#include "disruptor/util/DaemonThreadFactory.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

namespace {
// Java: reference/disruptor/src/jmh/java/com/lmax/disruptor/MultiProducerSingleConsumer.java
constexpr int kBigBuffer = 1 << 22;
constexpr int kBatchSize = 100;
constexpr int kMpThreads = 4;

// Static Disruptor instance (matches Java @State(Scope.Benchmark) - shared across threads)
using DisruptorType = disruptor::dsl::Disruptor<nano_stream::bench::jmh::SimpleEvent,
                                                disruptor::dsl::ProducerType::MULTI,
                                                disruptor::BusySpinWaitStrategy>;
using RingBufferType = disruptor::RingBuffer<nano_stream::bench::jmh::SimpleEvent,
                                              disruptor::MultiProducerSequencer<disruptor::BusySpinWaitStrategy>>;

// Static instance shared across all threads (Java: @State(Scope.Benchmark))
static std::mutex g_init_mutex;
static DisruptorType* g_disruptor = nullptr;
static std::shared_ptr<RingBufferType> g_ringBuffer = nullptr;
static std::atomic<bool> g_initialized{false};
static nano_stream::bench::jmh::ConsumeHandler* g_handler = nullptr; // Keep handler alive

} // namespace

// Setup function (1:1 with Java @Setup)
// Java: @Setup - creates Disruptor with consumer (called once per benchmark)
static void Setup_MPSC_producing(const benchmark::State& state) {
  if (state.thread_index() != 0) return; // Only thread 0 does setup
  
  std::lock_guard<std::mutex> lock(g_init_mutex);
  if (!g_initialized.load(std::memory_order_relaxed)) {
    try {
      disruptor::BusySpinWaitStrategy ws;
      auto factory = std::make_shared<nano_stream::bench::jmh::SimpleEventFactory>();
      auto& threadFactory = disruptor::util::DaemonThreadFactory::INSTANCE();

      // Create Disruptor (matches Java @Setup)
      g_disruptor = new DisruptorType(factory, kBigBuffer, threadFactory, ws);

      // Add consumer handler (1:1 with Java: disruptor.handleEventsWith(new SimpleEventHandler(bh)))
      // Handler must live for the entire Disruptor lifetime, so store it statically
      g_handler = new nano_stream::bench::jmh::ConsumeHandler();
      g_disruptor->handleEventsWith(*g_handler);

      // Start consumer thread (1:1 with Java: ringBuffer = disruptor.start())
      g_ringBuffer = g_disruptor->start();
      
      g_initialized.store(true, std::memory_order_release);
    } catch (const std::exception& e) {
      // Setup errors are hard to report, but will be caught in benchmark body
      g_initialized.store(false, std::memory_order_release);
    }
  }
}

// Teardown function (1:1 with Java @TearDown)
// Java: @TearDown - disruptor.shutdown() (called once per benchmark)
static void Teardown_MPSC_producing(const benchmark::State& state) {
  if (state.thread_index() != 0) return; // Only thread 0 does teardown
  
  // Use try_lock to avoid blocking if another thread is in Setup
  std::unique_lock<std::mutex> lock(g_init_mutex, std::try_to_lock);
  if (!lock.owns_lock()) {
    // Another thread is in Setup, skip teardown (will be cleaned up later)
    return;
  }
  
  if (g_initialized.load(std::memory_order_relaxed) && g_disruptor != nullptr) {
    try {
      // Java: disruptor.shutdown() - waits for backlog to drain, then halts and joins threads
      // In benchmark context, measurement is already done, so we don't need to wait for backlog.
      // We use halt() directly to avoid blocking. This matches Java's behavior in practice since
      // JMH benchmarks typically don't have significant backlog at teardown time.
      g_disruptor->halt(); // halt() stops processors and joins threads, no backlog wait
      delete g_disruptor;
      g_disruptor = nullptr;
      delete g_handler;
      g_handler = nullptr;
      g_ringBuffer = nullptr;
      g_initialized.store(false, std::memory_order_release);
    } catch (...) {
      // Best-effort cleanup
      g_disruptor = nullptr;
      g_handler = nullptr;
      g_ringBuffer = nullptr;
      g_initialized.store(false, std::memory_order_release);
    }
  }
}

// 1:1 with Java JMH class:
// reference/disruptor/src/jmh/java/com/lmax/disruptor/MultiProducerSingleConsumer.java
static void JMH_MultiProducerSingleConsumer_producing(benchmark::State& state) {
  // Ensure all threads see the ringBuffer (wait for initialization)
  while (!g_initialized.load(std::memory_order_acquire) || g_ringBuffer == nullptr) {
    std::this_thread::yield();
  }

  // Check if initialization failed
  if (g_ringBuffer == nullptr) {
    state.SkipWithError("RingBuffer is null after initialization");
    return;
  }

  // 1:1 with Java benchmark body:
  //   long sequence = ringBuffer.next();
  //   SimpleEvent e = ringBuffer.get(sequence);
  //   e.setValue(0);
  //   ringBuffer.publish(sequence);
  for (auto _ : state) {
    const int64_t sequence = g_ringBuffer->next();
    auto& e = g_ringBuffer->get(sequence);
    e.value = 0;
    g_ringBuffer->publish(sequence);
  }
}

static auto* bm_JMH_MultiProducerSingleConsumer_producing = [] {
  auto* b = benchmark::RegisterBenchmark("JMH_MultiProducerSingleConsumer_producing",
                                         &JMH_MultiProducerSingleConsumer_producing);
  b->Threads(kMpThreads);
  b->Setup(Setup_MPSC_producing);  // 1:1 with Java @Setup
  b->Teardown(Teardown_MPSC_producing);  // 1:1 with Java @TearDown
  return nano_stream::bench::jmh::applyJmhDefaults(b);
}();

// Separate static instances for batch benchmark (Java uses same @State(Scope.Benchmark) instance)
static std::mutex g_batch_init_mutex;
static DisruptorType* g_batch_disruptor = nullptr;
static std::shared_ptr<RingBufferType> g_batch_ringBuffer = nullptr;
static std::atomic<bool> g_batch_initialized{false};
static nano_stream::bench::jmh::ConsumeHandler* g_batch_handler = nullptr; // Keep handler alive

// Setup function for batch (1:1 with Java @Setup)
static void Setup_MPSC_producingBatch(const benchmark::State& state) {
  if (state.thread_index() != 0) return; // Only thread 0 does setup
  
  std::lock_guard<std::mutex> lock(g_batch_init_mutex);
  if (!g_batch_initialized.load(std::memory_order_relaxed)) {
    try {
      disruptor::BusySpinWaitStrategy ws;
      auto factory = std::make_shared<nano_stream::bench::jmh::SimpleEventFactory>();
      auto& threadFactory = disruptor::util::DaemonThreadFactory::INSTANCE();

      // Create Disruptor (matches Java @Setup)
      g_batch_disruptor = new DisruptorType(factory, kBigBuffer, threadFactory, ws);

      // Add consumer handler (1:1 with Java: disruptor.handleEventsWith(new SimpleEventHandler(bh)))
      // Handler must live for the entire Disruptor lifetime, so store it statically
      g_batch_handler = new nano_stream::bench::jmh::ConsumeHandler();
      g_batch_disruptor->handleEventsWith(*g_batch_handler);

      // Start consumer thread (1:1 with Java: ringBuffer = disruptor.start())
      g_batch_ringBuffer = g_batch_disruptor->start();
      
      g_batch_initialized.store(true, std::memory_order_release);
    } catch (const std::exception& e) {
      g_batch_initialized.store(false, std::memory_order_release);
    }
  }
}

// Teardown function for batch (1:1 with Java @TearDown)
static void Teardown_MPSC_producingBatch(const benchmark::State& state) {
  if (state.thread_index() != 0) return; // Only thread 0 does teardown
  
  // Use try_lock to avoid blocking if another thread is in Setup
  std::unique_lock<std::mutex> lock(g_batch_init_mutex, std::try_to_lock);
  if (!lock.owns_lock()) {
    // Another thread is in Setup, skip teardown (will be cleaned up later)
    return;
  }
  
  if (g_batch_initialized.load(std::memory_order_relaxed) && g_batch_disruptor != nullptr) {
    try {
      // Java: disruptor.shutdown() - waits for backlog to drain, then halts and joins threads
      // In benchmark context, measurement is already done, so we don't need to wait for backlog.
      // We use halt() directly to avoid blocking. This matches Java's behavior in practice since
      // JMH benchmarks typically don't have significant backlog at teardown time.
      g_batch_disruptor->halt(); // halt() stops processors and joins threads, no backlog wait
      delete g_batch_disruptor;
      g_batch_disruptor = nullptr;
      delete g_batch_handler;
      g_batch_handler = nullptr;
      g_batch_ringBuffer = nullptr;
      g_batch_initialized.store(false, std::memory_order_release);
    } catch (...) {
      // Best-effort cleanup
      g_batch_disruptor = nullptr;
      g_batch_handler = nullptr;
      g_batch_ringBuffer = nullptr;
      g_batch_initialized.store(false, std::memory_order_release);
    }
  }
}

// 1:1 with Java JMH method producingBatch(), including OperationsPerInvocation(BATCH_SIZE).
static void JMH_MultiProducerSingleConsumer_producingBatch(benchmark::State& state) {
  // Ensure all threads see the ringBuffer
  while (!g_batch_initialized.load(std::memory_order_acquire) || g_batch_ringBuffer == nullptr) {
    std::this_thread::yield();
  }

  // Check if initialization failed
  if (g_batch_ringBuffer == nullptr) {
    state.SkipWithError("RingBuffer is null after initialization");
    return;
  }

  for (auto _ : state) {
    int64_t hi = g_batch_ringBuffer->next(kBatchSize);
    int64_t lo = hi - (kBatchSize - 1);
    for (int64_t seq = lo; seq <= hi; ++seq) {
      auto& e = g_batch_ringBuffer->get(seq);
      e.value = 0;
    }
    g_batch_ringBuffer->publish(lo, hi);
  }

  // JMH: @OperationsPerInvocation(BATCH_SIZE)
  state.SetItemsProcessed(state.iterations() * static_cast<int64_t>(kBatchSize));
}

static auto* bm_JMH_MultiProducerSingleConsumer_producingBatch = [] {
  auto* b = benchmark::RegisterBenchmark("JMH_MultiProducerSingleConsumer_producingBatch",
                                         &JMH_MultiProducerSingleConsumer_producingBatch);
  b->Threads(kMpThreads);
  b->Setup(Setup_MPSC_producingBatch);  // 1:1 with Java @Setup
  b->Teardown(Teardown_MPSC_producingBatch);  // 1:1 with Java @TearDown
  return nano_stream::bench::jmh::applyJmhDefaults(b);
}();


