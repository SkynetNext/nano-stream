#pragma once

#include <cstddef>
#include <cstdint>
#include <new>

namespace aeron {
namespace util {

/**
 * Cache line size for alignment purposes.
 */
static constexpr std::size_t CACHE_LINE_LENGTH =
    std::hardware_destructive_interference_size;

/**
 * Bit manipulation utilities inspired by Aeron's BitUtil.
 */
namespace BitUtil {

/**
 * Check if a value is a power of two.
 */
constexpr bool is_power_of_two(std::size_t value) noexcept {
  return value > 0 && (value & (value - 1)) == 0;
}

/**
 * Find the next power of two >= value.
 */
constexpr std::size_t find_next_power_of_two(std::size_t value) noexcept {
  if (value == 0)
    return 1;

  --value;
  value |= value >> 1;
  value |= value >> 2;
  value |= value >> 4;
  value |= value >> 8;
  value |= value >> 16;

  if constexpr (sizeof(std::size_t) > 4) {
    value |= value >> 32;
  }

  return ++value;
}

/**
 * Align a value to the next boundary.
 */
constexpr std::size_t align(std::size_t value, std::size_t alignment) noexcept {
  return (value + (alignment - 1)) & ~(alignment - 1);
}

/**
 * Check if a value is aligned to boundary.
 */
constexpr bool is_aligned(std::size_t value, std::size_t alignment) noexcept {
  return (value & (alignment - 1)) == 0;
}

/**
 * Count leading zeros.
 */
inline int leading_zeros(std::uint32_t value) noexcept {
  if (value == 0)
    return 32;

#if defined(__GNUC__) || defined(__clang__)
  return __builtin_clz(value);
#elif defined(_MSC_VER)
  unsigned long index;
  _BitScanReverse(&index, value);
  return 31 - static_cast<int>(index);
#else
  // Fallback implementation
  int count = 0;
  for (int i = 31; i >= 0; --i) {
    if (value & (1U << i))
      break;
    ++count;
  }
  return count;
#endif
}

/**
 * Count leading zeros for 64-bit.
 */
inline int leading_zeros(std::uint64_t value) noexcept {
  if (value == 0)
    return 64;

#if defined(__GNUC__) || defined(__clang__)
  return __builtin_clzll(value);
#elif defined(_MSC_VER)
  unsigned long index;
  _BitScanReverse64(&index, value);
  return 63 - static_cast<int>(index);
#else
  // Fallback implementation
  int count = 0;
  for (int i = 63; i >= 0; --i) {
    if (value & (1ULL << i))
      break;
    ++count;
  }
  return count;
#endif
}

/**
 * Count trailing zeros.
 */
inline int trailing_zeros(std::uint32_t value) noexcept {
  if (value == 0)
    return 32;

#if defined(__GNUC__) || defined(__clang__)
  return __builtin_ctz(value);
#elif defined(_MSC_VER)
  unsigned long index;
  _BitScanForward(&index, value);
  return static_cast<int>(index);
#else
  // Fallback implementation
  int count = 0;
  for (int i = 0; i < 32; ++i) {
    if (value & (1U << i))
      break;
    ++count;
  }
  return count;
#endif
}

/**
 * Count trailing zeros for 64-bit.
 */
inline int trailing_zeros(std::uint64_t value) noexcept {
  if (value == 0)
    return 64;

#if defined(__GNUC__) || defined(__clang__)
  return __builtin_ctzll(value);
#elif defined(_MSC_VER)
  unsigned long index;
  _BitScanForward64(&index, value);
  return static_cast<int>(index);
#else
  // Fallback implementation
  int count = 0;
  for (int i = 0; i < 64; ++i) {
    if (value & (1ULL << i))
      break;
    ++count;
  }
  return count;
#endif
}

/**
 * Fast integer log2.
 */
inline int fast_log2(std::uint32_t value) noexcept {
  return 31 - leading_zeros(value);
}

/**
 * Fast integer log2 for 64-bit.
 */
inline int fast_log2(std::uint64_t value) noexcept {
  return 63 - leading_zeros(value);
}

/**
 * Reverse the bits in a 32-bit integer.
 */
inline std::uint32_t reverse_bits(std::uint32_t value) noexcept {
  value = ((value & 0xAAAAAAAA) >> 1) | ((value & 0x55555555) << 1);
  value = ((value & 0xCCCCCCCC) >> 2) | ((value & 0x33333333) << 2);
  value = ((value & 0xF0F0F0F0) >> 4) | ((value & 0x0F0F0F0F) << 4);
  value = ((value & 0xFF00FF00) >> 8) | ((value & 0x00FF00FF) << 8);
  return (value >> 16) | (value << 16);
}

/**
 * Population count (number of 1 bits).
 */
inline int popcount(std::uint32_t value) noexcept {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_popcount(value);
#elif defined(_MSC_VER)
  return __popcnt(value);
#else
  // Fallback implementation
  value = value - ((value >> 1) & 0x55555555);
  value = (value & 0x33333333) + ((value >> 2) & 0x33333333);
  return (((value + (value >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
#endif
}

/**
 * Population count for 64-bit.
 */
inline int popcount(std::uint64_t value) noexcept {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_popcountll(value);
#elif defined(_MSC_VER)
  return __popcnt64(value);
#else
  // Fallback implementation
  return popcount(static_cast<std::uint32_t>(value)) +
         popcount(static_cast<std::uint32_t>(value >> 32));
#endif
}

/**
 * Byte swap operations.
 */
inline std::uint16_t byte_swap(std::uint16_t value) noexcept {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_bswap16(value);
#elif defined(_MSC_VER)
  return _byteswap_ushort(value);
#else
  return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
#endif
}

inline std::uint32_t byte_swap(std::uint32_t value) noexcept {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_bswap32(value);
#elif defined(_MSC_VER)
  return _byteswap_ulong(value);
#else
  return ((value & 0xFF) << 24) | (((value >> 8) & 0xFF) << 16) |
         (((value >> 16) & 0xFF) << 8) | ((value >> 24) & 0xFF);
#endif
}

inline std::uint64_t byte_swap(std::uint64_t value) noexcept {
#if defined(__GNUC__) || defined(__clang__)
  return __builtin_bswap64(value);
#elif defined(_MSC_VER)
  return _byteswap_uint64(value);
#else
  return ((value & 0xFFULL) << 56) | (((value >> 8) & 0xFFULL) << 48) |
         (((value >> 16) & 0xFFULL) << 40) | (((value >> 24) & 0xFFULL) << 32) |
         (((value >> 32) & 0xFFULL) << 24) | (((value >> 40) & 0xFFULL) << 16) |
         (((value >> 48) & 0xFFULL) << 8) | ((value >> 56) & 0xFFULL);
#endif
}

} // namespace BitUtil

} // namespace util
} // namespace aeron
