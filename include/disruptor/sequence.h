#pragma once

#include <atomic>
#include <cstdint>
#include <new>

namespace disruptor {

/**
 * Cache-line aligned atomic sequence number for high-performance lock-free
 * operations.
 *
 * This class is inspired by LMAX Disruptor's Sequence class and provides:
 * - Cache line padding to avoid false sharing
 * - Atomic operations for thread-safe access
 * - Memory ordering semantics for optimal performance
 */
class alignas(std::hardware_destructive_interference_size) Sequence {
public:
  /// Initial value for sequences
  static constexpr int64_t INITIAL_VALUE = -1L;

private:
  // Left padding to prevent false sharing
  alignas(
      std::hardware_destructive_interference_size) std::atomic<int64_t> value_;

  // Right padding is automatically handled by the alignas directive

public:
  /**
   * Create a sequence initialized to INITIAL_VALUE (-1).
   */
  Sequence() noexcept : value_(INITIAL_VALUE) {}

  /**
   * Create a sequence with a specified initial value.
   *
   * @param initial_value The initial value for this sequence.
   */
  explicit Sequence(int64_t initial_value) noexcept : value_(initial_value) {}

  // Non-copyable and non-movable to prevent accidental copying
  Sequence(const Sequence &) = delete;
  Sequence &operator=(const Sequence &) = delete;
  Sequence(Sequence &&) = delete;
  Sequence &operator=(Sequence &&) = delete;

  /**
   * Perform a relaxed read of this sequence's value.
   *
   * @return The current value of the sequence.
   */
  int64_t get() const noexcept {
    return value_.load(std::memory_order_acquire);
  }

  /**
   * Perform an ordered write of this sequence. Uses release ordering
   * to ensure all previous writes are visible before this write.
   *
   * @param new_value The new value for the sequence.
   */
  void set(int64_t new_value) noexcept {
    value_.store(new_value, std::memory_order_release);
  }

  /**
   * Perform a sequentially consistent write of this sequence.
   * This provides stronger ordering guarantees than set().
   *
   * @param new_value The new value for the sequence.
   */
  void set_volatile(int64_t new_value) noexcept {
    value_.store(new_value, std::memory_order_seq_cst);
  }

  /**
   * Perform a compare and swap operation on the sequence.
   *
   * @param expected_value The expected current value.
   * @param new_value The value to update to.
   * @return true if the operation succeeds, false otherwise.
   */
  bool compare_and_set(int64_t expected_value, int64_t new_value) noexcept {
    return value_.compare_exchange_strong(expected_value, new_value,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire);
  }

  /**
   * Atomically increment the sequence by one.
   *
   * @return The value after the increment.
   */
  int64_t increment_and_get() noexcept { return add_and_get(1); }

  /**
   * Atomically add the supplied value to the sequence.
   *
   * @param increment The value to add to the sequence.
   * @return The value after the increment.
   */
  int64_t add_and_get(int64_t increment) noexcept {
    return value_.fetch_add(increment, std::memory_order_acq_rel) + increment;
  }

  /**
   * Atomically add the supplied value to the sequence.
   *
   * @param increment The value to add to the sequence.
   * @return The value before the increment.
   */
  int64_t get_and_add(int64_t increment) noexcept {
    return value_.fetch_add(increment, std::memory_order_acq_rel);
  }

  /**
   * Get the raw atomic reference for advanced operations.
   * Use with caution as this bypasses the class interface.
   */
  std::atomic<int64_t> &get_atomic() noexcept { return value_; }

  /**
   * Get the const raw atomic reference for advanced operations.
   */
  const std::atomic<int64_t> &get_atomic() const noexcept { return value_; }
};

static_assert(sizeof(Sequence) >= std::hardware_destructive_interference_size,
              "Sequence must be at least one cache line size");

} // namespace disruptor
