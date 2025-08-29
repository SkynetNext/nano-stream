#pragma once

#include "sequence.h"
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace nano_stream {

/**
 * Strategy employed for making consumers wait on a cursor sequence.
 * Inspired by LMAX Disruptor's WaitStrategy.
 */
class WaitStrategy {
public:
  virtual ~WaitStrategy() = default;

  /**
   * Wait for the given sequence to be available.
   *
   * @param sequence The sequence to wait for
   * @param cursor The main sequence from ring buffer
   * @param dependent_sequence The dependent sequence to wait on
   * @return The sequence that is available (may be greater than requested)
   */
  virtual int64_t wait_for(int64_t sequence, const Sequence &cursor,
                           int64_t dependent_sequence) = 0;

  /**
   * Signal all waiting consumers that the cursor has advanced.
   */
  virtual void signal_all_when_blocking() = 0;
};

/**
 * Busy spin wait strategy - lowest latency, highest CPU usage.
 * Use when you need the absolute lowest latency and can afford high CPU usage.
 */
class BusySpinWaitStrategy : public WaitStrategy {
public:
  int64_t wait_for(int64_t sequence, const Sequence &cursor,
                   int64_t dependent_sequence) override {
    while (dependent_sequence < sequence) {
      // Busy spin - do nothing, just check again
      dependent_sequence = cursor.get();
    }
    return dependent_sequence;
  }

  void signal_all_when_blocking() override {
    // No-op for busy spin - no blocking to signal
  }
};

/**
 * Yielding wait strategy - balanced latency and CPU usage.
 * Yields CPU to other threads when waiting.
 */
class YieldingWaitStrategy : public WaitStrategy {
public:
  int64_t wait_for(int64_t sequence, const Sequence &cursor,
                   int64_t dependent_sequence) override {
    int64_t counter = 100;
    while (dependent_sequence < sequence) {
      counter = apply_wait_method(counter);
      dependent_sequence = cursor.get();
    }
    return dependent_sequence;
  }

  void signal_all_when_blocking() override {
    // No-op for yielding - no blocking to signal
  }

private:
  int64_t apply_wait_method(int64_t counter) {
    if (counter > 100) {
      --counter;
    } else if (counter > 0) {
      --counter;
      std::this_thread::yield();
    } else {
      std::this_thread::yield();
    }
    return counter;
  }
};

/**
 * Sleeping wait strategy - lower CPU usage, higher latency.
 * Uses sleep with exponential backoff.
 */
class SleepingWaitStrategy : public WaitStrategy {
public:
  int64_t wait_for(int64_t sequence, const Sequence &cursor,
                   int64_t dependent_sequence) override {
    int64_t counter = 200;
    while (dependent_sequence < sequence) {
      counter = apply_wait_method(counter);
      dependent_sequence = cursor.get();
    }
    return dependent_sequence;
  }

  void signal_all_when_blocking() override {
    // No-op for sleeping - no blocking to signal
  }

private:
  int64_t apply_wait_method(int64_t counter) {
    if (counter > 100) {
      --counter;
    } else if (counter > 0) {
      --counter;
      std::this_thread::sleep_for(std::chrono::microseconds(1));
    } else {
      std::this_thread::sleep_for(std::chrono::microseconds(1000));
    }
    return counter;
  }
};

/**
 * Blocking wait strategy - lowest CPU usage, highest latency.
 * Uses condition variables for blocking.
 */
class BlockingWaitStrategy : public WaitStrategy {
private:
  mutable std::mutex mutex_;
  mutable std::condition_variable condition_variable_;

public:
  int64_t wait_for(int64_t sequence, const Sequence &cursor,
                   int64_t dependent_sequence) override {
    if (dependent_sequence < sequence) {
      std::unique_lock<std::mutex> lock(mutex_);
      while (dependent_sequence < sequence) {
        condition_variable_.wait(lock);
        dependent_sequence = cursor.get();
      }
    }
    return dependent_sequence;
  }

  void signal_all_when_blocking() override { condition_variable_.notify_all(); }
};

/**
 * Timeout blocking wait strategy - blocking with timeout.
 */
class TimeoutBlockingWaitStrategy : public WaitStrategy {
private:
  mutable std::mutex mutex_;
  mutable std::condition_variable condition_variable_;
  std::chrono::milliseconds timeout_;

public:
  explicit TimeoutBlockingWaitStrategy(std::chrono::milliseconds timeout)
      : timeout_(timeout) {}

  int64_t wait_for(int64_t sequence, const Sequence &cursor,
                   int64_t dependent_sequence) override {
    if (dependent_sequence < sequence) {
      std::unique_lock<std::mutex> lock(mutex_);
      while (dependent_sequence < sequence) {
        if (condition_variable_.wait_for(lock, timeout_) ==
            std::cv_status::timeout) {
          // Return current available sequence on timeout
          return cursor.get();
        }
        dependent_sequence = cursor.get();
      }
    }
    return dependent_sequence;
  }

  void signal_all_when_blocking() override { condition_variable_.notify_all(); }
};

} // namespace nano_stream
