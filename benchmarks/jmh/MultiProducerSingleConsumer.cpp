#include <benchmark/benchmark.h>

#include "jmh_config.h"
#include "jmh_util.h"

#include "disruptor/BusySpinWaitStrategy.h"
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

// 1:1 with Java JMH class:
// reference/disruptor/src/jmh/java/com/lmax/disruptor/MultiProducerSingleConsumer.java
static void JMH_MultiProducerSingleConsumer_producing(benchmark::State& state) {
  // Java: @Setup - creates Disruptor with consumer (called once per benchmark)
  //   disruptor = new Disruptor<>(SimpleEvent::new, BIG_BUFFER, DaemonThreadFactory.INSTANCE,
  //                               ProducerType.MULTI, new BusySpinWaitStrategy());
  //   disruptor.handleEventsWith(new SimpleEventHandler(bh));
  //   ringBuffer = disruptor.start();
  
  // Initialize Disruptor once (thread-safe)
  if (!g_initialized.load(std::memory_order_acquire)) {
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
        state.SkipWithError(("Failed to initialize Disruptor: " + std::string(e.what())).c_str());
        return;
      }
    }
  }

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

  // Java: @TearDown - disruptor.shutdown() (called once per benchmark)
  // Note: Google Benchmark doesn't have perfect teardown signal, so we use a heuristic
  // We shutdown when we've done enough iterations (warmup + measurement)
  // Actually, we'll let the static destructor handle cleanup, or use a global teardown
  // For now, skip explicit teardown to avoid issues with multiple benchmark runs
}

static auto* bm_JMH_MultiProducerSingleConsumer_producing = [] {
  auto* b = benchmark::RegisterBenchmark("JMH_MultiProducerSingleConsumer_producing",
                                         &JMH_MultiProducerSingleConsumer_producing);
  b->Threads(kMpThreads);
  return nano_stream::bench::jmh::applyJmhDefaults(b);
}();

// 1:1 with Java JMH method producingBatch(), including OperationsPerInvocation(BATCH_SIZE).
static void JMH_MultiProducerSingleConsumer_producingBatch(benchmark::State& state) {
  // Java: @Setup - same as producing() (uses same @State(Scope.Benchmark) instance)
  // Use separate static instances for batch benchmark
  static std::mutex g_batch_init_mutex;
  static DisruptorType* g_batch_disruptor = nullptr;
  static std::shared_ptr<RingBufferType> g_batch_ringBuffer = nullptr;
  static std::atomic<bool> g_batch_initialized{false};
  static nano_stream::bench::jmh::ConsumeHandler* g_batch_handler = nullptr; // Keep handler alive

  // Initialize Disruptor once (thread-safe)
  if (!g_batch_initialized.load(std::memory_order_acquire)) {
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
        state.SkipWithError(("Failed to initialize Disruptor: " + std::string(e.what())).c_str());
        return;
      }
    }
  }

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

  // Java: @TearDown - disruptor.shutdown()
  // Skip explicit teardown to avoid issues with multiple benchmark runs
}

static auto* bm_JMH_MultiProducerSingleConsumer_producingBatch = [] {
  auto* b = benchmark::RegisterBenchmark("JMH_MultiProducerSingleConsumer_producingBatch",
                                         &JMH_MultiProducerSingleConsumer_producingBatch);
  b->Threads(kMpThreads);
  return nano_stream::bench::jmh::applyJmhDefaults(b);
}();


