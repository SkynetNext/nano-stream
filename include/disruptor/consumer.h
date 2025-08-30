#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "event_handler.h"
#include "ring_buffer.h"
#include "sequence.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <thread>

namespace disruptor {

/**
 * Error codes for consumer operations
 */
enum class ConsumerError {
  SUCCESS = 0,
  ALREADY_RUNNING = 1,
  NOT_RUNNING = 2,
  INVALID_ARGUMENT = 3,
  HANDLER_ERROR = 4
};

/**
 * Interface for handling exceptions in consumers.
 * Note: This is kept for backward compatibility but should be avoided in
 * performance-critical code.
 */
template <typename T> class ExceptionHandler {
public:
  virtual ~ExceptionHandler() = default;
  virtual void handle_exception(const std::exception &e, T &event,
                                int64_t sequence) = 0;
};

/**
 * Default exception handler that does nothing.
 */
template <typename T>
class DefaultExceptionHandler : public ExceptionHandler<T> {
public:
  void handle_exception(const std::exception &e, T &event,
                        int64_t sequence) override {
    // Do nothing - this is the no-exception approach
  }
};

/**
 * High-performance event consumer that processes events from a ring buffer.
 *
 * This consumer provides:
 * - Lock-free event processing
 * - Batch processing for improved throughput
 * - Configurable batch size and timeout
 * - Performance counters
 * - Error code based error handling (no exceptions)
 */
template <typename T> class Consumer {
private:
  RingBuffer<T> &ring_buffer_;
  std::unique_ptr<EventHandler<T>> event_handler_;
  std::unique_ptr<ExceptionHandler<T>> exception_handler_;

  // Consumer sequence
  Sequence sequence_;

  // Configuration
  size_t batch_size_;
  std::chrono::milliseconds timeout_;

  // State
  std::atomic<bool> running_;
  std::thread consumer_thread_;

  // Performance counters
  std::atomic<int64_t> events_processed_;
  std::atomic<int64_t> batches_processed_;

  /**
   * Main consumption loop.
   */
  void consume_loop() {
    int64_t next_sequence = sequence_.get() + 1;

    while (running_.load(std::memory_order_acquire)) {
      int64_t available_sequence = ring_buffer_.get_cursor();

      if (next_sequence <= available_sequence) {
        // Process available events
        int64_t batch_end_calc =
            next_sequence + static_cast<int64_t>(batch_size_) - 1;
        int64_t batch_end = (batch_end_calc < available_sequence)
                                ? batch_end_calc
                                : available_sequence;
        auto result = process_batch(next_sequence, batch_end);

        if (result != ConsumerError::SUCCESS) {
          // Handle error - in no-exception approach, we just log or continue
          break;
        }

        next_sequence = batch_end + 1;
        sequence_.set(batch_end);
      } else {
        // No events available, wait
        std::this_thread::sleep_for(timeout_);
      }
    }
  }

  /**
   * Process a batch of events.
   */
  ConsumerError process_batch(int64_t start_sequence, int64_t end_sequence) {
    if (start_sequence > end_sequence) {
      return ConsumerError::INVALID_ARGUMENT;
    }

    int64_t current_sequence = start_sequence;
    int64_t batch_size = end_sequence - start_sequence + 1;

    for (int64_t i = 0; i < batch_size; ++i) {
      T &event = ring_buffer_.get(current_sequence);
      bool end_of_batch = (i == batch_size - 1);

      event_handler_->on_event(event, current_sequence, end_of_batch);
      events_processed_.fetch_add(1, std::memory_order_relaxed);

      current_sequence++;
    }

    batches_processed_.fetch_add(1, std::memory_order_relaxed);
    return ConsumerError::SUCCESS;
  }

public:
  /**
   * Create a consumer with the specified configuration.
   *
   * @param ring_buffer The ring buffer to consume from.
   * @param event_handler The event handler to process events.
   * @param batch_size Number of events to process in each batch.
   * @param timeout Timeout when no events are available.
   */
  Consumer(RingBuffer<T> &ring_buffer,
           std::unique_ptr<EventHandler<T>> event_handler,
           size_t batch_size = 10,
           std::chrono::milliseconds timeout = std::chrono::milliseconds(10))
      : ring_buffer_(ring_buffer), event_handler_(std::move(event_handler)),
        exception_handler_(std::make_unique<DefaultExceptionHandler<T>>()),
        sequence_(Sequence::INITIAL_VALUE), batch_size_(batch_size),
        timeout_(timeout), running_(false), events_processed_(0),
        batches_processed_(0) {

    // Add this consumer's sequence to the ring buffer's gating sequences
    ring_buffer_.add_gating_sequences({sequence_});
  }

  // Non-copyable
  Consumer(const Consumer &) = delete;
  Consumer &operator=(const Consumer &) = delete;

  // Movable
  Consumer(Consumer &&) = default;
  Consumer &operator=(Consumer &&) = default;

  /**
   * Set a custom exception handler.
   *
   * @param handler The exception handler to use.
   */
  void set_exception_handler(std::unique_ptr<ExceptionHandler<T>> handler) {
    if (handler) {
      exception_handler_ = std::move(handler);
    }
  }

  /**
   * Start consuming events.
   *
   * @return ConsumerError::SUCCESS on success, ConsumerError::ALREADY_RUNNING
   * if already running.
   */
  ConsumerError start() {
    if (running_.load(std::memory_order_acquire)) {
      return ConsumerError::ALREADY_RUNNING;
    }

    running_.store(true, std::memory_order_release);
    consumer_thread_ = std::thread(&Consumer::consume_loop, this);
    return ConsumerError::SUCCESS;
  }

  /**
   * Stop consuming events.
   *
   * @return ConsumerError::SUCCESS on success, ConsumerError::NOT_RUNNING if
   * not running.
   */
  ConsumerError stop() {
    if (!running_.load(std::memory_order_acquire)) {
      return ConsumerError::NOT_RUNNING;
    }

    running_.store(false, std::memory_order_release);

    if (consumer_thread_.joinable()) {
      consumer_thread_.join();
    }

    return ConsumerError::SUCCESS;
  }

  /**
   * Check if the consumer is currently running.
   *
   * @return true if running, false otherwise.
   */
  bool is_running() const { return running_.load(std::memory_order_acquire); }

  /**
   * Get the current sequence position.
   *
   * @return The current sequence number.
   */
  int64_t get_sequence() const { return sequence_.get(); }

  /**
   * Get the number of events processed.
   *
   * @return The total number of events processed.
   */
  int64_t get_events_processed() const {
    return events_processed_.load(std::memory_order_acquire);
  }

  /**
   * Get the number of batches processed.
   *
   * @return The total number of batches processed.
   */
  int64_t get_batches_processed() const {
    return batches_processed_.load(std::memory_order_acquire);
  }

  /**
   * Reset performance counters.
   */
  void reset_counters() {
    events_processed_.store(0, std::memory_order_release);
    batches_processed_.store(0, std::memory_order_release);
  }

  /**
   * Get the batch size.
   *
   * @return The current batch size.
   */
  size_t get_batch_size() const { return batch_size_; }

  /**
   * Set the batch size.
   *
   * @param batch_size The new batch size.
   */
  void set_batch_size(size_t batch_size) { batch_size_ = batch_size; }

  /**
   * Get the timeout.
   *
   * @return The current timeout.
   */
  std::chrono::milliseconds get_timeout() const { return timeout_; }

  /**
   * Set the timeout.
   *
   * @param timeout The new timeout.
   */
  void set_timeout(std::chrono::milliseconds timeout) { timeout_ = timeout; }

  /**
   * Destructor - ensures the consumer is stopped.
   */
  ~Consumer() { stop(); }
};

} // namespace disruptor
