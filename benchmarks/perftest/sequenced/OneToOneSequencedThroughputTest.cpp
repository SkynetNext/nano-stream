// 1:1 port of com.lmax.disruptor.sequenced.OneToOneSequencedThroughputTest
// Source: reference/disruptor/src/perftest/java/com/lmax/disruptor/sequenced/OneToOneSequencedThroughputTest.java
//
// UniCast a series of items between 1 publisher and 1 event processor.
//
// +----+    +-----+
// | P1 |--->| EP1 |
// +----+    +-----+
//
// Disruptor:
// ==========
//              track to prevent wrap
//              +------------------+
//              |                  |
//              |                  v
// +----+    +====+    +====+   +-----+
// | P1 |--->| RB |<---| SB |   | EP1 |
// +----+    +====+    +====+   +-----+
//      claim      get    ^        |
//                        |        |
//                        +--------+
//                          waitFor
//
// P1  - Publisher 1
// RB  - RingBuffer
// SB  - SequenceBarrier
// EP1 - EventProcessor 1

#include <benchmark/benchmark.h>

#include "disruptor/BatchEventProcessor.h"
#include "disruptor/BatchEventProcessorBuilder.h"
#include "disruptor/BusySpinWaitStrategy.h"
#include "disruptor/RingBuffer.h"
#include "disruptor/YieldingWaitStrategy.h"

#include "perftest/support/PerfTestContext.h"
#include "perftest/support/PerfTestUtil.h"
#include "perftest/support/ValueAdditionEventHandler.h"
#include "perftest/support/ValueEvent.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <exception>

namespace {

constexpr int kBufferSize = 1024 * 64;
constexpr int64_t kIterations = 1000L * 1000L * 100L; // 100 million

} // namespace

static void PerfTest_OneToOneSequencedThroughputTest(benchmark::State& state) {
  using namespace nano_stream::bench::perftest;
  using namespace nano_stream::bench::perftest::support;

  const int64_t expectedResult = accumulatedAddition(kIterations);

  // Run test
  for (auto _ : state) {
    try {
      // Setup for each iteration (Java creates new test instance each run)
      disruptor::YieldingWaitStrategy ws;
      auto ringBuffer = disruptor::SingleProducerRingBuffer<
          ValueEvent, disruptor::YieldingWaitStrategy>::createSingleProducer(
          ValueEvent::EVENT_FACTORY, kBufferSize, ws);

      auto sequenceBarrier = ringBuffer->newBarrier();
      auto handler = std::make_shared<ValueAdditionEventHandler>();
      
      disruptor::BatchEventProcessorBuilder builder;
      auto batchEventProcessor = builder.build(*ringBuffer, *sequenceBarrier, *handler);

      ringBuffer->addGatingSequences(batchEventProcessor->getSequence());

      PerfTestContext perfTestContext;
      
      // Use atomic bool as latch (CountDownLatch equivalent)
      auto latch = std::make_shared<std::atomic<bool>>(false);
      
      // Java: expectedCount = batchEventProcessor.getSequence().get() + ITERATIONS
      // Sequence starts at -1 (INITIAL_CURSOR_VALUE), so after kIterations publishes:
      // sequence will be -1 + kIterations = kIterations - 1
      int64_t currentSequence = batchEventProcessor->getSequence().get();
      int64_t expectedCount = currentSequence + kIterations;
      handler->reset(latch, expectedCount);

      std::atomic<bool> threadError{false};
      std::string threadErrorMsg;
      
      // Start processor in separate thread (capture by value to ensure lifetime)
      std::thread processorThread([batchEventProcessor, &threadError, &threadErrorMsg]() {
        try {
          batchEventProcessor->run();
        } catch (const std::exception& e) {
          threadError = true;
          threadErrorMsg = e.what();
        } catch (...) {
          threadError = true;
          threadErrorMsg = "Unknown exception";
        }
      });

      auto start = std::chrono::steady_clock::now();

      // Producer loop
      // Java: for (long i = 0; i < ITERATIONS; i++) { long next = rb.next(); rb.get(next).setValue(i); rb.publish(next); }
      for (int64_t i = 0; i < kIterations; i++) {
        int64_t next = ringBuffer->next();
        ringBuffer->get(next).setValue(i);
        ringBuffer->publish(next);
      }

      // Wait for completion (latch equivalent)
      while (!latch->load(std::memory_order_acquire)) {
        if (threadError.load()) {
          throw std::runtime_error("Processor thread error: " + threadErrorMsg);
        }
        std::this_thread::yield();
      }

    auto end = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                         end - start)
                         .count();
    if (elapsedMs == 0) elapsedMs = 1; // Avoid division by zero

    perfTestContext.setDisruptorOps((kIterations * 1000L) / elapsedMs);
    perfTestContext.setBatchData(handler->getBatchesProcessed(), kIterations);

    // Wait for event processor sequence
    while (batchEventProcessor->getSequence().get() != expectedCount) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

      batchEventProcessor->halt();
      
      // Wait for processor thread to finish
      if (processorThread.joinable()) {
        processorThread.join();
      }

      // Verify result
      failIfNot(expectedResult, handler->getValue());

      // Report metrics
      state.counters["ops_per_sec"] =
          benchmark::Counter(perfTestContext.getDisruptorOps(),
                             benchmark::Counter::kIsRate);
      state.counters["batch_percent"] =
          benchmark::Counter(perfTestContext.getBatchPercent() * 100.0);
      state.counters["avg_batch_size"] =
          benchmark::Counter(perfTestContext.getAverageBatchSize());
    } catch (const std::exception& e) {
      throw;
    } catch (...) {
      throw;
    }
  }
}

BENCHMARK(PerfTest_OneToOneSequencedThroughputTest)
    ->Unit(benchmark::kMillisecond);

