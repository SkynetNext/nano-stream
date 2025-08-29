#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace nano_stream {

/**
 * Event handler interface for processing events in the ring buffer.
 * Inspired by LMAX Disruptor's EventHandler.
 */
template <typename T> class EventHandler {
public:
  virtual ~EventHandler() = default;

  /**
   * Called when a publisher has published an event to the ring buffer.
   *
   * @param event The event that was published
   * @param sequence The sequence of the event
   * @param end_of_batch Flag indicating if this is the last event in a batch
   */
  virtual void on_event(T &event, int64_t sequence, bool end_of_batch) = 0;
};

/**
 * Lambda-based event handler for convenience.
 */
template <typename T> class LambdaEventHandler : public EventHandler<T> {
private:
  std::function<void(T &, int64_t, bool)> handler_fn_;

public:
  explicit LambdaEventHandler(
      std::function<void(T &, int64_t, bool)> handler_fn)
      : handler_fn_(std::move(handler_fn)) {}

  void on_event(T &event, int64_t sequence, bool end_of_batch) override {
    handler_fn_(event, sequence, end_of_batch);
  }
};

/**
 * Batch event handler for processing multiple events at once.
 */
template <typename T> class BatchEventHandler : public EventHandler<T> {
public:
  virtual ~BatchEventHandler() = default;

  /**
   * Called when a batch of events is available for processing.
   *
   * @param events Array of events in the batch
   * @param batch_size Number of events in the batch
   * @param first_sequence Sequence number of the first event
   */
  virtual void on_batch(T *events, size_t batch_size,
                        int64_t first_sequence) = 0;

  void on_event(T &event, int64_t sequence, bool end_of_batch) override {
    // Default implementation calls on_batch for single events
    on_batch(&event, 1, sequence);
  }
};

} // namespace nano_stream
