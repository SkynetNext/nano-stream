#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace aeron {
namespace concurrent {

/**
 * Bounds checking utility for buffer operations.
 */
#ifndef DISABLE_BOUNDS_CHECKS
#define AERON_BOUNDS_CHECK(index, length, capacity)                            \
  do {                                                                         \
    if ((index) < 0 ||                                                         \
        static_cast<std::size_t>(index) + (length) > (capacity)) {             \
      throw std::out_of_range("Index out of bounds");                          \
    }                                                                          \
  } while (0)
#else
#define AERON_BOUNDS_CHECK(index, length, capacity) ((void)0)
#endif

/**
 * Atomic buffer providing thread-safe access to a memory region.
 * Inspired by Aeron's AtomicBuffer with C++ memory ordering semantics.
 */
class AtomicBuffer {
public:
  using index_t = std::ptrdiff_t;

  /**
   * Default constructor - creates an empty buffer.
   */
  constexpr AtomicBuffer() noexcept = default;

  /**
   * Wrap a buffer of memory for a given length.
   */
  AtomicBuffer(std::uint8_t *buffer, std::size_t length) noexcept
      : buffer_(buffer), length_(length) {}

  /**
   * Wrap a buffer and initialize with a value.
   */
  AtomicBuffer(std::uint8_t *buffer, std::size_t length,
               std::uint8_t initial_value)
      : buffer_(buffer), length_(length) {
    set_memory(0, length, initial_value);
  }

  // Default copy/move semantics
  AtomicBuffer(const AtomicBuffer &) = default;
  AtomicBuffer &operator=(const AtomicBuffer &) = default;
  AtomicBuffer(AtomicBuffer &&) = default;
  AtomicBuffer &operator=(AtomicBuffer &&) = default;

  /**
   * Wrap an existing buffer.
   */
  void wrap(const AtomicBuffer &buffer) noexcept {
    buffer_ = buffer.buffer_;
    length_ = buffer.length_;
  }

  void wrap(std::uint8_t *buffer, std::size_t length) noexcept {
    buffer_ = buffer;
    length_ = length;
  }

  /**
   * Get the capacity of the buffer.
   */
  std::size_t capacity() const noexcept { return length_; }

  /**
   * Get the raw buffer pointer.
   */
  std::uint8_t *buffer() const noexcept { return buffer_; }

  /**
   * Check if the buffer is valid.
   */
  bool is_valid() const noexcept { return buffer_ != nullptr && length_ > 0; }

  // ========== 64-bit Operations ==========

  void put_int64(index_t offset, std::int64_t value) const {
    bounds_check(offset, sizeof(std::int64_t));
    *reinterpret_cast<std::int64_t *>(buffer_ + offset) = value;
  }

  std::int64_t get_int64(index_t offset) const {
    bounds_check(offset, sizeof(std::int64_t));
    return *reinterpret_cast<const std::int64_t *>(buffer_ + offset);
  }

  void put_int64_ordered(index_t offset, std::int64_t value) const {
    bounds_check(offset, sizeof(std::int64_t));
    std::atomic<std::int64_t> *ptr =
        reinterpret_cast<std::atomic<std::int64_t> *>(buffer_ + offset);
    ptr->store(value, std::memory_order_release);
  }

  std::int64_t get_int64_volatile(index_t offset) const {
    bounds_check(offset, sizeof(std::int64_t));
    const std::atomic<std::int64_t> *ptr =
        reinterpret_cast<const std::atomic<std::int64_t> *>(buffer_ + offset);
    return ptr->load(std::memory_order_acquire);
  }

  void put_int64_atomic(index_t offset, std::int64_t value) const {
    bounds_check(offset, sizeof(std::int64_t));
    std::atomic<std::int64_t> *ptr =
        reinterpret_cast<std::atomic<std::int64_t> *>(buffer_ + offset);
    ptr->store(value, std::memory_order_seq_cst);
  }

  bool compare_and_set_int64(index_t offset, std::int64_t expected,
                             std::int64_t update) const {
    bounds_check(offset, sizeof(std::int64_t));
    std::atomic<std::int64_t> *ptr =
        reinterpret_cast<std::atomic<std::int64_t> *>(buffer_ + offset);
    return ptr->compare_exchange_strong(
        expected, update, std::memory_order_acq_rel, std::memory_order_acquire);
  }

  std::int64_t get_and_set_int64(index_t offset, std::int64_t value) const {
    bounds_check(offset, sizeof(std::int64_t));
    std::atomic<std::int64_t> *ptr =
        reinterpret_cast<std::atomic<std::int64_t> *>(buffer_ + offset);
    return ptr->exchange(value, std::memory_order_acq_rel);
  }

  std::int64_t get_and_add_int64(index_t offset, std::int64_t delta) const {
    bounds_check(offset, sizeof(std::int64_t));
    std::atomic<std::int64_t> *ptr =
        reinterpret_cast<std::atomic<std::int64_t> *>(buffer_ + offset);
    return ptr->fetch_add(delta, std::memory_order_acq_rel);
  }

  void add_int64_ordered(index_t offset, std::int64_t delta) const {
    const std::int64_t value = get_int64(offset);
    put_int64_ordered(offset, value + delta);
  }

  // ========== 32-bit Operations ==========

  void put_int32(index_t offset, std::int32_t value) const {
    bounds_check(offset, sizeof(std::int32_t));
    *reinterpret_cast<std::int32_t *>(buffer_ + offset) = value;
  }

  std::int32_t get_int32(index_t offset) const {
    bounds_check(offset, sizeof(std::int32_t));
    return *reinterpret_cast<const std::int32_t *>(buffer_ + offset);
  }

  void put_int32_ordered(index_t offset, std::int32_t value) const {
    bounds_check(offset, sizeof(std::int32_t));
    std::atomic<std::int32_t> *ptr =
        reinterpret_cast<std::atomic<std::int32_t> *>(buffer_ + offset);
    ptr->store(value, std::memory_order_release);
  }

  std::int32_t get_int32_volatile(index_t offset) const {
    bounds_check(offset, sizeof(std::int32_t));
    const std::atomic<std::int32_t> *ptr =
        reinterpret_cast<const std::atomic<std::int32_t> *>(buffer_ + offset);
    return ptr->load(std::memory_order_acquire);
  }

  void put_int32_atomic(index_t offset, std::int32_t value) const {
    bounds_check(offset, sizeof(std::int32_t));
    std::atomic<std::int32_t> *ptr =
        reinterpret_cast<std::atomic<std::int32_t> *>(buffer_ + offset);
    ptr->store(value, std::memory_order_seq_cst);
  }

  bool compare_and_set_int32(index_t offset, std::int32_t expected,
                             std::int32_t update) const {
    bounds_check(offset, sizeof(std::int32_t));
    std::atomic<std::int32_t> *ptr =
        reinterpret_cast<std::atomic<std::int32_t> *>(buffer_ + offset);
    return ptr->compare_exchange_strong(
        expected, update, std::memory_order_acq_rel, std::memory_order_acquire);
  }

  std::int32_t get_and_set_int32(index_t offset, std::int32_t value) const {
    bounds_check(offset, sizeof(std::int32_t));
    std::atomic<std::int32_t> *ptr =
        reinterpret_cast<std::atomic<std::int32_t> *>(buffer_ + offset);
    return ptr->exchange(value, std::memory_order_acq_rel);
  }

  std::int32_t get_and_add_int32(index_t offset, std::int32_t delta) const {
    bounds_check(offset, sizeof(std::int32_t));
    std::atomic<std::int32_t> *ptr =
        reinterpret_cast<std::atomic<std::int32_t> *>(buffer_ + offset);
    return ptr->fetch_add(delta, std::memory_order_acq_rel);
  }

  void add_int32_ordered(index_t offset, std::int32_t delta) const {
    const std::int32_t value = get_int32(offset);
    put_int32_ordered(offset, value + delta);
  }

  // ========== 16-bit Operations ==========

  void put_int16(index_t offset, std::int16_t value) const {
    bounds_check(offset, sizeof(std::int16_t));
    *reinterpret_cast<std::int16_t *>(buffer_ + offset) = value;
  }

  std::int16_t get_int16(index_t offset) const {
    bounds_check(offset, sizeof(std::int16_t));
    return *reinterpret_cast<const std::int16_t *>(buffer_ + offset);
  }

  void put_uint16(index_t offset, std::uint16_t value) const {
    bounds_check(offset, sizeof(std::uint16_t));
    *reinterpret_cast<std::uint16_t *>(buffer_ + offset) = value;
  }

  std::uint16_t get_uint16(index_t offset) const {
    bounds_check(offset, sizeof(std::uint16_t));
    return *reinterpret_cast<const std::uint16_t *>(buffer_ + offset);
  }

  // ========== 8-bit Operations ==========

  void put_uint8(index_t offset, std::uint8_t value) const {
    bounds_check(offset, sizeof(std::uint8_t));
    *(buffer_ + offset) = value;
  }

  std::uint8_t get_uint8(index_t offset) const {
    bounds_check(offset, sizeof(std::uint8_t));
    return *(buffer_ + offset);
  }

  // ========== Memory Operations ==========

  void put_bytes(index_t offset, const AtomicBuffer &src_buffer,
                 index_t src_offset, std::size_t length) const {
    bounds_check(offset, length);
    src_buffer.bounds_check(src_offset, length);
    std::memcpy(buffer_ + offset, src_buffer.buffer_ + src_offset, length);
  }

  void put_bytes(index_t offset, const std::uint8_t *src,
                 std::size_t length) const {
    bounds_check(offset, length);
    std::memcpy(buffer_ + offset, src, length);
  }

  void get_bytes(index_t offset, std::uint8_t *dst, std::size_t length) const {
    bounds_check(offset, length);
    std::memcpy(dst, buffer_ + offset, length);
  }

  void set_memory(index_t offset, std::size_t length,
                  std::uint8_t value) const {
    bounds_check(offset, length);
    std::memset(buffer_ + offset, value, length);
  }

  // ========== String Operations ==========

  std::string get_string(index_t offset) const {
    std::int32_t length;
    bounds_check(offset, sizeof(length));
    std::memcpy(&length, buffer_ + offset, sizeof(length));
    return get_string_without_length(offset + sizeof(std::int32_t),
                                     static_cast<std::size_t>(length));
  }

  std::string get_string_without_length(index_t offset,
                                        std::size_t length) const {
    bounds_check(offset, length);
    return std::string(reinterpret_cast<const char *>(buffer_ + offset),
                       length);
  }

  std::int32_t get_string_length(index_t offset) const {
    std::int32_t length;
    bounds_check(offset, sizeof(length));
    std::memcpy(&length, buffer_ + offset, sizeof(length));
    return length;
  }

  std::int32_t put_string(index_t offset, const std::string &value) const {
    auto length = static_cast<std::int32_t>(value.length());
    bounds_check(offset, value.length() + sizeof(length));

    std::memcpy(buffer_ + offset, &length, sizeof(length));
    std::memcpy(buffer_ + offset + sizeof(length), value.c_str(),
                value.length());

    return sizeof(std::int32_t) + length;
  }

  std::int32_t put_string_without_length(index_t offset,
                                         const std::string &value) const {
    bounds_check(offset, value.length());
    std::memcpy(buffer_ + offset, value.c_str(), value.length());
    return static_cast<std::int32_t>(value.length());
  }

  // ========== Template Operations for Struct Overlay ==========

  template <typename T> T &overlay_struct(index_t offset) const {
    bounds_check(offset, sizeof(T));
    return *reinterpret_cast<T *>(buffer_ + offset);
  }

  template <typename T> const T &overlay_struct(index_t offset) const {
    bounds_check(offset, sizeof(T));
    return *reinterpret_cast<const T *>(buffer_ + offset);
  }

private:
  void bounds_check(index_t offset, std::size_t length) const {
    AERON_BOUNDS_CHECK(offset, length, length_);
  }

  std::uint8_t *buffer_ = nullptr;
  std::size_t length_ = 0;
};

} // namespace concurrent
} // namespace aeron
