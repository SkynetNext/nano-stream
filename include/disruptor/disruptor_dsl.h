#pragma once

#include "batch_event_processor.h"
#include "event_handler.h"
#include "ring_buffer.h"
#include "sequence_barrier.h"
#include <functional>
#include <memory>
#include <thread>
#include <vector>

namespace disruptor {

/**
 * Forward declaration
 */
template <typename T> class EventHandlerGroup;

/**
 * Main Disruptor class that provides a DSL-style API for setting up the
 * disruptor pattern. This is the primary interface for creating complex
 * consumer dependency graphs.
 */
template <typename T> class Disruptor {
private:
  RingBuffer<T> &ring_buffer_;
  std::vector<std::unique_ptr<BatchEventProcessor<T>>> event_processors_;
  std::vector<std::unique_ptr<EventHandler<T>>> event_handlers_;
  std::atomic<bool> started_{false};

public:
  /**
   * Create a new Disruptor.
   *
   * @param ring_buffer The ring buffer to use
   */
  explicit Disruptor(RingBuffer<T> &ring_buffer) : ring_buffer_(ring_buffer) {}

  /**
   * Set up event handlers to handle events from the ring buffer.
   * These handlers will process events in parallel.
   *
   * @param handlers Event handlers to process events
   * @return EventHandlerGroup for chaining dependencies
   */
  template <typename... Handlers>
  EventHandlerGroup<T> handle_events_with(Handlers &&...handlers) {
    static_assert(sizeof...(handlers) > 0,
                  "At least one handler must be provided");

    std::vector<std::unique_ptr<EventHandler<T>>> handler_vector;
    (handler_vector.emplace_back(std::forward<Handlers>(handlers)), ...);

    return create_event_processors({}, std::move(handler_vector));
  }

  /**
   * Set up event handlers to handle events from the ring buffer.
   * These handlers will process events in parallel.
   *
   * @param handlers Vector of event handlers to process events
   * @return EventHandlerGroup for chaining dependencies
   */
  EventHandlerGroup<T>
  handle_events_with(std::vector<std::unique_ptr<EventHandler<T>>> handlers) {
    return create_event_processors({}, std::move(handlers));
  }

  /**
   * Start the Disruptor and all event processors.
   */
  void start() {
    if (started_.load()) {
      return;
    }

    for (auto &processor : event_processors_) {
      processor->start();
    }

    started_.store(true);
  }

  /**
   * Stop the Disruptor and all event processors.
   */
  void stop() {
    if (!started_.load()) {
      return;
    }

    for (auto &processor : event_processors_) {
      processor->stop();
    }

    started_.store(false);
  }

  /**
   * Check if the Disruptor is started.
   */
  bool is_started() const { return started_.load(); }

  /**
   * Get the ring buffer.
   */
  RingBuffer<T> &get_ring_buffer() { return ring_buffer_; }

  /**
   * Get the ring buffer (const version).
   */
  const RingBuffer<T> &get_ring_buffer() const { return ring_buffer_; }

private:
  /**
   * Create event processors with dependencies.
   */
  EventHandlerGroup<T> create_event_processors(
      const std::vector<std::reference_wrapper<const Sequence>>
          &dependent_sequences,
      std::vector<std::unique_ptr<EventHandler<T>>> handlers) {

    std::vector<std::unique_ptr<BatchEventProcessor<T>>> processors;
    std::vector<std::reference_wrapper<const Sequence>> sequences;

    for (auto &handler : handlers) {
      // Create sequence barrier with dependencies
      auto barrier = ring_buffer_.new_barrier(dependent_sequences);

      // Create event processor
      auto processor = std::make_unique<BatchEventProcessor<T>>(
          std::move(barrier), std::move(handler), &ring_buffer_);

      // Store processor and its sequence
      sequences.emplace_back(processor->get_sequence());
      processors.push_back(std::move(processor));
    }

    // Add processors to the main list
    for (auto &processor : processors) {
      event_processors_.push_back(std::move(processor));
    }

    // Add handlers to the main list (they were moved to processors)
    for (auto &handler : handlers) {
      event_handlers_.push_back(std::move(handler));
    }

    return EventHandlerGroup<T>(*this, sequences);
  }

  // Friend declaration for EventHandlerGroup
  friend class EventHandlerGroup<T>;
};

/**
 * Group of event handlers that can be used to chain dependencies.
 */
template <typename T> class EventHandlerGroup {
private:
  Disruptor<T> &disruptor_;
  std::vector<std::reference_wrapper<const Sequence>> sequences_;

public:
  EventHandlerGroup(
      Disruptor<T> &disruptor,
      const std::vector<std::reference_wrapper<const Sequence>> &sequences)
      : disruptor_(disruptor), sequences_(sequences) {}

  /**
   * Set up event handlers to handle events after the current group has
   * processed them. This creates a dependency where the new handlers wait for
   * this group to complete.
   *
   * @param handlers Event handlers to process events
   * @return EventHandlerGroup for further chaining
   */
  template <typename... Handlers>
  EventHandlerGroup<T> then(Handlers &&...handlers) {
    static_assert(sizeof...(handlers) > 0,
                  "At least one handler must be provided");

    std::vector<std::unique_ptr<EventHandler<T>>> handler_vector;
    (handler_vector.emplace_back(std::forward<Handlers>(handlers)), ...);

    return disruptor_.create_event_processors(sequences_,
                                              std::move(handler_vector));
  }

  /**
   * Set up event handlers to handle events after the current group has
   * processed them.
   *
   * @param handlers Vector of event handlers to process events
   * @return EventHandlerGroup for further chaining
   */
  EventHandlerGroup<T>
  then(std::vector<std::unique_ptr<EventHandler<T>>> handlers) {
    return disruptor_.create_event_processors(sequences_, std::move(handlers));
  }

  /**
   * Create a sequence barrier that includes all the processors in this group.
   * This allows custom event processors to have dependencies on processors
   * created by the disruptor.
   *
   * @return A sequence barrier including all the processors in this group
   */
  std::unique_ptr<SequenceBarrier> as_sequence_barrier() {
    return disruptor_.ring_buffer_.new_barrier(sequences_);
  }
};

} // namespace disruptor
