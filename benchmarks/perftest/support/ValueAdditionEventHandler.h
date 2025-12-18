#pragma once
// 1:1 port of com.lmax.disruptor.support.ValueAdditionEventHandler
// Source: reference/disruptor/src/perftest/java/com/lmax/disruptor/support/ValueAdditionEventHandler.java

#include "disruptor/EventHandler.h"
#include "disruptor/Sequence.h"
#include "ValueEvent.h"

#include <atomic>
#include <cstdint>
#include <memory>

namespace nano_stream::bench::perftest::support {

class ValueAdditionEventHandler : public disruptor::EventHandler<ValueEvent> {
public:
  ValueAdditionEventHandler()
      : value_(0), batchesProcessed_(0), count_(0), latch_(nullptr) {}

  int64_t getValue() const { return value_.load(std::memory_order_acquire); }

  int64_t getBatchesProcessed() const {
    return batchesProcessed_.load(std::memory_order_acquire);
  }

  void reset(std::shared_ptr<std::atomic<bool>> latch, int64_t expectedCount) {
    value_.store(0, std::memory_order_release);
    latch_ = latch;
    count_ = expectedCount;
    batchesProcessed_.store(0, std::memory_order_release);
  }

  void onEvent(ValueEvent& event, int64_t sequence,
               bool endOfBatch) override {
    // Read event value immediately (before any potential overwrite)
    // Java: value.set(value.get() + event.getValue());
    int64_t eventValue = event.getValue();
    value_.fetch_add(eventValue, std::memory_order_acq_rel);

    if (count_ == sequence) {
      if (latch_) {
        latch_->store(true, std::memory_order_release);
      }
    }
  }

  void onBatchStart(int64_t batchSize, int64_t queueDepth) override {
    batchesProcessed_.fetch_add(1, std::memory_order_acq_rel);
  }

private:
  std::atomic<int64_t> value_;
  std::atomic<int64_t> batchesProcessed_;
  int64_t count_;
  std::shared_ptr<std::atomic<bool>> latch_;
};

} // namespace nano_stream::bench::perftest::support

