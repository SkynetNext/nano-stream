#pragma once

#include "sequence.h"
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <new>
#include <stdexcept>
#include <thread>
#include <vector>

namespace nano_stream {

/**
 * Exception thrown when there is insufficient capacity in the ring buffer.
 */
class InsufficientCapacityException : public std::runtime_error {
public:
  InsufficientCapacityException()
      : std::runtime_error("Insufficient capacity in ring buffer") {}
};

/**
 * Factory interface for creating events in the ring buffer.
 */
template <typename T> class EventFactory {
public:
  virtual ~EventFactory() = default;
  virtual T create() = 0;
};

/**
 * Simple lambda-based event factory.
 */
template <typename T> class LambdaEventFactory : public EventFactory<T> {
private:
  std::function<T()> factory_fn_;

public:
  explicit LambdaEventFactory(std::function<T()> factory_fn)
      : factory_fn_(std::move(factory_fn)) {}

  T create() override { return factory_fn_(); }
};

/**
 * High-performance lock-free ring buffer inspired by LMAX Disruptor.
 *
 * This ring buffer provides:
 * - Lock-free single-producer operations
 * - Cache-line optimization to reduce false sharing
 * - Pre-allocated entries to avoid allocation during operation
 * - Sequence-based coordination between producers and consumers
 */
template <typename T>
class alignas(std::hardware_destructive_interference_size) RingBuffer {
private:
  static constexpr size_t BUFFER_PAD = 32;

  const size_t buffer_size_;
  const size_t index_mask_;
  std::unique_ptr<T[]> entries_;

  // Sequence for tracking the current position
  alignas(std::hardware_destructive_interference_size) Sequence cursor_;

  // Next value to be published (for single producer)
  alignas(std::hardware_destructive_interference_size)
      std::atomic<int64_t> next_value_;

  // Cached minimum sequence from gating sequences
  alignas(std::hardware_destructive_interference_size)
      std::atomic<int64_t> cached_value_;

  // Gating sequences for consumers
  std::vector<std::reference_wrapper<const Sequence>> gating_sequences_;

  /**
   * Check if the buffer size is valid (power of 2).
   */
  static bool is_power_of_two(size_t value) {
    return value > 0 && std::has_single_bit(value);
  }

  /**
   * Get the minimum sequence from all gating sequences.
   */
  int64_t get_minimum_sequence() const {
    if (gating_sequences_.empty()) {
      return cursor_.get();
    }

    int64_t min_seq = gating_sequences_[0].get().get();
    for (size_t i = 1; i < gating_sequences_.size(); ++i) {
      int64_t seq = gating_sequences_[i].get().get();
      if (seq < min_seq) {
        min_seq = seq;
      }
    }
    return min_seq;
  }

  /**
   * Check if there is available capacity without updating cached values.
   */
  bool has_available_capacity_fast(int required_capacity) const {
    int64_t next_val = next_value_.load(std::memory_order_relaxed);
    int64_t wrap_point =
        (next_val + required_capacity) - static_cast<int64_t>(buffer_size_);
    int64_t cached_gating = cached_value_.load(std::memory_order_acquire);

    return wrap_point <= cached_gating;
  }

public:
  /// Initial cursor value
  static constexpr int64_t INITIAL_CURSOR_VALUE = Sequence::INITIAL_VALUE;

  /**
   * Create a ring buffer with the specified size and event factory.
   *
   * @param buffer_size Number of elements in the ring buffer (must be power of
   * 2).
   * @param event_factory Factory for creating events.
   */
  template <typename Factory>
  RingBuffer(size_t buffer_size, Factory &event_factory)
      : buffer_size_(buffer_size), index_mask_(buffer_size - 1),
        entries_(std::make_unique<T[]>(buffer_size + 2 * BUFFER_PAD)),
        cursor_(INITIAL_CURSOR_VALUE), next_value_(INITIAL_CURSOR_VALUE),
        cached_value_(INITIAL_CURSOR_VALUE) {
    if (buffer_size < 1) {
      throw std::invalid_argument("Buffer size must not be less than 1");
    }
    if (!is_power_of_two(buffer_size)) {
      throw std::invalid_argument("Buffer size must be a power of 2");
    }

    // Initialize all entries using the factory
    for (size_t i = 0; i < buffer_size_; ++i) {
      entries_[BUFFER_PAD + i] = event_factory.create();
    }
  }

  /**
   * Convenience constructor using lambda factory.
   */
  template <typename FactoryFn>
  RingBuffer(size_t buffer_size, FactoryFn factory_fn)
      : buffer_size_(buffer_size), index_mask_(buffer_size - 1),
        entries_(std::make_unique<T[]>(buffer_size + 2 * BUFFER_PAD)),
        cursor_(INITIAL_CURSOR_VALUE), next_value_(INITIAL_CURSOR_VALUE),
        cached_value_(INITIAL_CURSOR_VALUE) {
    if (buffer_size < 1) {
      throw std::invalid_argument("Buffer size must not be less than 1");
    }
    if (!is_power_of_two(buffer_size)) {
      throw std::invalid_argument("Buffer size must be a power of 2");
    }

    // Initialize all entries using the lambda factory
    for (size_t i = 0; i < buffer_size_; ++i) {
      entries_[BUFFER_PAD + i] = factory_fn();
    }
  }

  // Non-copyable and non-movable
  RingBuffer(const RingBuffer &) = delete;
  RingBuffer &operator=(const RingBuffer &) = delete;
  RingBuffer(RingBuffer &&) = delete;
  RingBuffer &operator=(RingBuffer &&) = delete;

  /**
   * Get the event at the specified sequence.
   *
   * @param sequence The sequence number.
   * @return Reference to the event at the specified sequence.
   */
  T &get(int64_t sequence) {
    return entries_[BUFFER_PAD + (sequence & index_mask_)];
  }

  /**
   * Get the event at the specified sequence (const version).
   */
  const T &get(int64_t sequence) const {
    return entries_[BUFFER_PAD + (sequence & index_mask_)];
  }

  /**
   * Claim the next sequence for publishing.
   * This may block if there is insufficient capacity.
   *
   * @return The sequence number to publish to.
   */
  int64_t next() { return next(1); }

  /**
   * Claim the next n sequences for publishing.
   *
   * @param n Number of sequences to claim.
   * @return The highest sequence number claimed.
   */
  int64_t next(int n) {
    if (n < 1 || n > static_cast<int>(buffer_size_)) {
      throw std::invalid_argument("n must be > 0 and <= buffer_size");
    }

    int64_t current_next = next_value_.load(std::memory_order_relaxed);
    int64_t next_sequence = current_next + n;
    int64_t wrap_point = next_sequence - static_cast<int64_t>(buffer_size_);
    int64_t cached_gating = cached_value_.load(std::memory_order_acquire);

    if (wrap_point > cached_gating || cached_gating > current_next) {
      // Update cursor for consumers to see our progress
      cursor_.set(current_next);

      // Spin until space becomes available
      int64_t min_sequence;
      while (wrap_point > (min_sequence = get_minimum_sequence())) {
        std::this_thread::yield();
      }

      cached_value_.store(min_sequence, std::memory_order_release);
    }

    next_value_.store(next_sequence, std::memory_order_release);
    return next_sequence;
  }

  /**
   * Try to claim the next sequence without blocking.
   *
   * @return The sequence number to publish to.
   * @throws InsufficientCapacityException if no space is available.
   */
  int64_t try_next() { return try_next(1); }

  /**
   * Try to claim the next n sequences without blocking.
   *
   * @param n Number of sequences to claim.
   * @return The highest sequence number claimed.
   * @throws InsufficientCapacityException if no space is available.
   */
  int64_t try_next(int n) {
    if (n < 1) {
      throw std::invalid_argument("n must be > 0");
    }

    if (!has_available_capacity(n)) {
      throw InsufficientCapacityException();
    }

    int64_t next_sequence =
        next_value_.fetch_add(n, std::memory_order_acq_rel) + n;
    return next_sequence;
  }

  /**
   * Publish the event at the specified sequence.
   *
   * @param sequence The sequence to publish.
   */
  void publish(int64_t sequence) { cursor_.set(sequence); }

  /**
   * Publish a range of sequences.
   *
   * @param lo The lowest sequence to publish.
   * @param hi The highest sequence to publish.
   */
  void publish([[maybe_unused]] int64_t lo, int64_t hi) { publish(hi); }

  /**
   * Check if there is available capacity for the required number of entries.
   *
   * @param required_capacity The number of entries required.
   * @return true if space is available, false otherwise.
   */
  bool has_available_capacity(int required_capacity) {
    if (has_available_capacity_fast(required_capacity)) {
      return true;
    }

    // Update cached value and check again
    int64_t min_sequence = get_minimum_sequence();
    cached_value_.store(min_sequence, std::memory_order_release);

    int64_t next_val = next_value_.load(std::memory_order_acquire);
    int64_t wrap_point =
        (next_val + required_capacity) - static_cast<int64_t>(buffer_size_);

    return wrap_point <= min_sequence;
  }

  /**
   * Get the current cursor value.
   *
   * @return The current cursor position.
   */
  int64_t get_cursor() const { return cursor_.get(); }

  /**
   * Get the buffer size.
   *
   * @return The size of the buffer.
   */
  size_t get_buffer_size() const { return buffer_size_; }

  /**
   * Get the remaining capacity.
   *
   * @return The number of slots remaining.
   */
  size_t remaining_capacity() {
    int64_t next_val = next_value_.load(std::memory_order_acquire);
    int64_t consumed = get_minimum_sequence();
    return buffer_size_ - (next_val - consumed);
  }

  /**
   * Add gating sequences for consumers.
   *
   * @param sequences The sequences to gate on.
   */
  void add_gating_sequences(
      std::initializer_list<std::reference_wrapper<const Sequence>> sequences) {
    for (const auto &seq : sequences) {
      gating_sequences_.push_back(seq);
    }
  }

  /**
   * Check if a sequence is available for consumption.
   *
   * @param sequence The sequence to check.
   * @return true if the sequence is available, false otherwise.
   */
  bool is_available(int64_t sequence) const {
    int64_t current_cursor = cursor_.get();
    return sequence <= current_cursor &&
           sequence > current_cursor - static_cast<int64_t>(buffer_size_);
  }
};

} // namespace nano_stream
