#pragma once

#include "consumer.h" // For ExceptionHandler
#include "event_handler.h"
#include "sequence_barrier.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>

namespace disruptor {

/**
 * High-performance batch event processor that processes events from a ring
 * buffer. This is the main event loop for handling events from the Disruptor.
 */
template <typename T> class BatchEventProcessor {
private:
  std::unique_ptr<SequenceBarrier> sequence_barrier_;
  std::unique_ptr<EventHandler<T>> event_handler_;
  std::unique_ptr<ExceptionHandler<T>> exception_handler_;

  // Ring buffer reference for accessing events
  RingBuffer<T> *ring_buffer_;

  // Consumer sequence
  Sequence sequence_;

  // State
  std::atomic<bool> running_{false};
  std::thread processor_thread_;

  // Performance counters
  std::atomic<int64_t> events_processed_{0};
  std::atomic<int64_t> batches_processed_{0};

  /**
   * Main processing loop.
   */
  void run() {
    int64_t next_sequence = sequence_.get() + 1;

    while (running_.load(std::memory_order_acquire)) {
      try {
        // Wait for the next sequence to be available
        int64_t available_sequence = sequence_barrier_->wait_for(next_sequence);

        if (next_sequence <= available_sequence) {
          // Process available events
          process_events(next_sequence, available_sequence);
          next_sequence = available_sequence + 1;
          sequence_.set(available_sequence);
        }
      } catch (const AlertException &) {
        // Always break on AlertException - this is the normal shutdown signal
        break;
      } catch (const std::exception &e) {
        // Handle other exceptions
        if (exception_handler_) {
          // Note: We don't have access to the current event here
          // In a real implementation, we would need to handle this differently
        }
        // Continue processing on other exceptions
      }
    }
  }

  /**
   * Process a batch of events.
   */
  void process_events(int64_t start_sequence, int64_t end_sequence) {
    for (int64_t sequence = start_sequence; sequence <= end_sequence;
         ++sequence) {
      try {
        // Get the event from the ring buffer
        T &event = ring_buffer_->get(sequence);
        bool end_of_batch = (sequence == end_sequence);

        event_handler_->on_event(event, sequence, end_of_batch);
        events_processed_.fetch_add(1, std::memory_order_relaxed);

      } catch (const std::exception &e) {
        if (exception_handler_) {
          T dummy_event;
          exception_handler_->handle_exception(e, dummy_event, sequence);
        }
      }
    }

    batches_processed_.fetch_add(1, std::memory_order_relaxed);
  }

public:
  /**
   * Create a batch event processor.
   *
   * @param sequence_barrier The sequence barrier to wait on
   * @param event_handler The event handler to process events
   * @param ring_buffer The ring buffer to get events from
   * @param exception_handler Optional exception handler
   */
  BatchEventProcessor(
      std::unique_ptr<SequenceBarrier> sequence_barrier,
      std::unique_ptr<EventHandler<T>> event_handler,
      RingBuffer<T> *ring_buffer,
      std::unique_ptr<ExceptionHandler<T>> exception_handler = nullptr)
      : sequence_barrier_(std::move(sequence_barrier)),
        event_handler_(std::move(event_handler)), ring_buffer_(ring_buffer),
        exception_handler_(std::move(exception_handler)) {
    if (!exception_handler_) {
      exception_handler_ = std::make_unique<DefaultExceptionHandler<T>>();
    }
  }

  /**
   * Start the processor in a separate thread.
   */
  void start() {
    if (running_.load()) {
      return;
    }

    running_.store(true);
    processor_thread_ = std::thread(&BatchEventProcessor::run, this);
  }

  /**
   * Stop the processor.
   */
  void stop() {
    if (!running_.load()) {
      return;
    }

    running_.store(false);
    sequence_barrier_->alert();

    if (processor_thread_.joinable()) {
      // Wait for thread to finish with timeout
      auto timeout = std::chrono::seconds(5);
      auto start = std::chrono::steady_clock::now();

      while (processor_thread_.joinable() &&
             std::chrono::steady_clock::now() - start < timeout) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
      }

      if (processor_thread_.joinable()) {
        // Force detach if thread doesn't respond
        processor_thread_.detach();
      } else {
        processor_thread_.join();
      }
    }
  }

  /**
   * Get the sequence of this processor.
   */
  const Sequence &get_sequence() const { return sequence_; }

  /**
   * Check if the processor is running.
   */
  bool is_running() const { return running_.load(); }

  /**
   * Get the number of events processed.
   */
  int64_t get_events_processed() const { return events_processed_.load(); }

  /**
   * Get the number of batches processed.
   */
  int64_t get_batches_processed() const { return batches_processed_.load(); }

  /**
   * Set the exception handler.
   */
  void set_exception_handler(
      std::unique_ptr<ExceptionHandler<T>> exception_handler) {
    exception_handler_ = std::move(exception_handler);
  }
};

} // namespace disruptor
