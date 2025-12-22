#pragma once
// 1:1 port of com.lmax.disruptor.PerfTestContext
// Source: reference/disruptor/src/perftest/java/com/lmax/disruptor/PerfTestContext.java

#include <cstdint>

namespace disruptor::bench::perftest {

class PerfTestContext {
public:
  PerfTestContext() : disruptorOps_(0), batchesProcessedCount_(0), iterations_(0) {}

  int64_t getDisruptorOps() const { return disruptorOps_; }
  void setDisruptorOps(int64_t ops) { disruptorOps_ = ops; }

  int64_t getBatchesProcessedCount() const { return batchesProcessedCount_; }

  double getBatchPercent() const {
    if (batchesProcessedCount_ == 0) {
      return 0.0;
    }
    return 1.0 - (static_cast<double>(batchesProcessedCount_) / iterations_);
  }

  double getAverageBatchSize() const {
    if (batchesProcessedCount_ == 0) {
      return -1.0;
    }
    return static_cast<double>(iterations_) / batchesProcessedCount_;
  }

  void setBatchData(int64_t batchesProcessedCount, int64_t iterations) {
    batchesProcessedCount_ = batchesProcessedCount;
    iterations_ = iterations;
  }

private:
  int64_t disruptorOps_;
  int64_t batchesProcessedCount_;
  int64_t iterations_;
};

} // namespace disruptor::bench::perftest

