#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "../../util/bit_util.h"
#include "../atomic_buffer.h"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace aeron {
namespace concurrent {
namespace logbuffer {

/**
 * Log buffer descriptor for managing term buffers and metadata.
 * Based on Aeron's LogBufferDescriptor with C++ implementation.
 */
namespace LogBufferDescriptor {

// Constants from Aeron Java implementation
static constexpr std::int32_t TERM_MIN_LENGTH = 64 * 1024;
static constexpr std::int32_t TERM_MAX_LENGTH = 1024 * 1024 * 1024;
static constexpr std::int32_t AERON_PAGE_MIN_SIZE = 4 * 1024;
static constexpr std::int32_t AERON_PAGE_MAX_SIZE = 1024 * 1024 * 1024;

static constexpr int PARTITION_COUNT = 3;

// Log buffer layout offsets
static constexpr std::size_t LOG_META_DATA_SECTION_INDEX = PARTITION_COUNT;

// Metadata structure layout (packed)
#pragma pack(push, 4)
struct LogMetaDataDefn {
  std::int64_t term_tail_counters[PARTITION_COUNT];
  std::int32_t active_term_count;
  std::int8_t
      pad1[(2 * util::CACHE_LINE_LENGTH) -
           ((PARTITION_COUNT * sizeof(std::int64_t)) + sizeof(std::int32_t))];
  std::int64_t end_of_stream_position;
  std::int32_t is_connected;
  std::int32_t active_transport_count;
  std::int8_t pad2[(2 * util::CACHE_LINE_LENGTH) -
                   (sizeof(std::int64_t) + (2 * sizeof(std::int32_t)))];
  std::int64_t correlation_id;
  std::int32_t initial_term_id;
  std::int32_t default_frame_header_length;
  std::int32_t mtu_length;
  std::int32_t term_length;
  std::int32_t page_size;
  std::int8_t pad3[(util::CACHE_LINE_LENGTH) - (7 * sizeof(std::int32_t))];
};
#pragma pack(pop)

// Offset calculations
static constexpr std::size_t TERM_TAIL_COUNTERS_OFFSET =
    offsetof(LogMetaDataDefn, term_tail_counters);
static constexpr std::size_t LOG_ACTIVE_TERM_COUNT_OFFSET =
    offsetof(LogMetaDataDefn, active_term_count);
static constexpr std::size_t LOG_END_OF_STREAM_POSITION_OFFSET =
    offsetof(LogMetaDataDefn, end_of_stream_position);
static constexpr std::size_t LOG_IS_CONNECTED_OFFSET =
    offsetof(LogMetaDataDefn, is_connected);
static constexpr std::size_t LOG_ACTIVE_TRANSPORT_COUNT_OFFSET =
    offsetof(LogMetaDataDefn, active_transport_count);
static constexpr std::size_t LOG_INITIAL_TERM_ID_OFFSET =
    offsetof(LogMetaDataDefn, initial_term_id);
static constexpr std::size_t LOG_DEFAULT_FRAME_HEADER_LENGTH_OFFSET =
    offsetof(LogMetaDataDefn, default_frame_header_length);
static constexpr std::size_t LOG_MTU_LENGTH_OFFSET =
    offsetof(LogMetaDataDefn, mtu_length);
static constexpr std::size_t LOG_TERM_LENGTH_OFFSET =
    offsetof(LogMetaDataDefn, term_length);
static constexpr std::size_t LOG_PAGE_SIZE_OFFSET =
    offsetof(LogMetaDataDefn, page_size);
static constexpr std::size_t LOG_DEFAULT_FRAME_HEADER_OFFSET =
    sizeof(LogMetaDataDefn);
static constexpr std::size_t LOG_META_DATA_LENGTH = 4 * 1024;

// Validation functions
inline void check_term_length(std::int32_t term_length) {
  if (term_length < TERM_MIN_LENGTH) {
    throw std::invalid_argument("term length less than min size of " +
                                std::to_string(TERM_MIN_LENGTH) +
                                ", length=" + std::to_string(term_length));
  }

  if (term_length > TERM_MAX_LENGTH) {
    throw std::invalid_argument("term length greater than max size of " +
                                std::to_string(TERM_MAX_LENGTH) +
                                ", length=" + std::to_string(term_length));
  }

  if (!util::BitUtil::is_power_of_two(static_cast<std::size_t>(term_length))) {
    throw std::invalid_argument("term length not a power of 2, length=" +
                                std::to_string(term_length));
  }
}

inline void check_page_size(std::int32_t page_size) {
  if (page_size < AERON_PAGE_MIN_SIZE) {
    throw std::invalid_argument("page size less than min size of " +
                                std::to_string(AERON_PAGE_MIN_SIZE) +
                                ", size=" + std::to_string(page_size));
  }

  if (page_size > AERON_PAGE_MAX_SIZE) {
    throw std::invalid_argument("page size greater than max size of " +
                                std::to_string(AERON_PAGE_MAX_SIZE) +
                                ", size=" + std::to_string(page_size));
  }

  if (!util::BitUtil::is_power_of_two(static_cast<std::size_t>(page_size))) {
    throw std::invalid_argument("page size not a power of 2, size=" +
                                std::to_string(page_size));
  }
}

// Metadata accessors
inline std::int32_t initial_term_id(const AtomicBuffer &log_meta_data_buffer) {
  return log_meta_data_buffer.get_int32(LOG_INITIAL_TERM_ID_OFFSET);
}

inline std::int32_t mtu_length(const AtomicBuffer &log_meta_data_buffer) {
  return log_meta_data_buffer.get_int32(LOG_MTU_LENGTH_OFFSET);
}

inline std::int32_t term_length(const AtomicBuffer &log_meta_data_buffer) {
  return log_meta_data_buffer.get_int32(LOG_TERM_LENGTH_OFFSET);
}

inline std::int32_t page_size(const AtomicBuffer &log_meta_data_buffer) {
  return log_meta_data_buffer.get_int32(LOG_PAGE_SIZE_OFFSET);
}

inline std::int32_t
active_term_count(const AtomicBuffer &log_meta_data_buffer) {
  return log_meta_data_buffer.get_int32_volatile(LOG_ACTIVE_TERM_COUNT_OFFSET);
}

inline void active_term_count_ordered(AtomicBuffer &log_meta_data_buffer,
                                      std::int32_t active_term_count) {
  log_meta_data_buffer.put_int32_ordered(LOG_ACTIVE_TERM_COUNT_OFFSET,
                                         active_term_count);
}

inline bool cas_active_term_count(AtomicBuffer &log_meta_data_buffer,
                                  std::int32_t expected_term_count,
                                  std::int32_t update_term_count) {
  return log_meta_data_buffer.compare_and_set_int32(
      LOG_ACTIVE_TERM_COUNT_OFFSET, expected_term_count, update_term_count);
}

// Connection state
inline bool is_connected(const AtomicBuffer &log_meta_data_buffer) noexcept {
  return log_meta_data_buffer.get_int32_volatile(LOG_IS_CONNECTED_OFFSET) == 1;
}

inline void is_connected(AtomicBuffer &log_meta_data_buffer,
                         bool connected) noexcept {
  log_meta_data_buffer.put_int32_ordered(LOG_IS_CONNECTED_OFFSET,
                                         connected ? 1 : 0);
}

inline std::int32_t
active_transport_count(AtomicBuffer &log_meta_data_buffer) noexcept {
  return log_meta_data_buffer.get_int32_volatile(
      LOG_ACTIVE_TRANSPORT_COUNT_OFFSET);
}

inline void active_transport_count(AtomicBuffer &log_meta_data_buffer,
                                   std::int32_t count) noexcept {
  log_meta_data_buffer.put_int32_ordered(LOG_ACTIVE_TRANSPORT_COUNT_OFFSET,
                                         count);
}

inline std::int64_t
end_of_stream_position(const AtomicBuffer &log_meta_data_buffer) noexcept {
  return log_meta_data_buffer.get_int64_volatile(
      LOG_END_OF_STREAM_POSITION_OFFSET);
}

inline void end_of_stream_position(AtomicBuffer &log_meta_data_buffer,
                                   std::int64_t position) noexcept {
  log_meta_data_buffer.put_int64_ordered(LOG_END_OF_STREAM_POSITION_OFFSET,
                                         position);
}

// Index calculations
inline int next_partition_index(int current_index) noexcept {
  return (current_index + 1) % PARTITION_COUNT;
}

inline int previous_partition_index(int current_index) noexcept {
  return (current_index + (PARTITION_COUNT - 1)) % PARTITION_COUNT;
}

inline std::int32_t compute_term_count(std::int32_t term_id,
                                       std::int32_t initial_term_id) noexcept {
  const std::int64_t difference = static_cast<std::int64_t>(term_id) -
                                  static_cast<std::int64_t>(initial_term_id);
  return static_cast<std::int32_t>(difference & 0xFFFFFFFF);
}

inline int index_by_term(std::int32_t initial_term_id,
                         std::int32_t active_term_id) noexcept {
  return compute_term_count(active_term_id, initial_term_id) % PARTITION_COUNT;
}

inline int index_by_term_count(std::int64_t term_count) noexcept {
  return static_cast<int>(term_count % PARTITION_COUNT);
}

inline int index_by_position(std::int64_t position,
                             std::int32_t position_bits_to_shift) noexcept {
  return static_cast<int>(
      (static_cast<std::uint64_t>(position) >> position_bits_to_shift) %
      PARTITION_COUNT);
}

// Position calculations
inline std::int64_t compute_position(std::int32_t active_term_id,
                                     std::int32_t term_offset,
                                     std::int32_t position_bits_to_shift,
                                     std::int32_t initial_term_id) noexcept {
  auto term_count = static_cast<std::int64_t>(
      compute_term_count(active_term_id, initial_term_id));
  return (term_count << position_bits_to_shift) + term_offset;
}

inline std::int64_t
compute_term_begin_position(std::int32_t active_term_id,
                            std::int32_t position_bits_to_shift,
                            std::int32_t initial_term_id) noexcept {
  auto term_count = static_cast<std::int64_t>(
      compute_term_count(active_term_id, initial_term_id));
  return term_count << position_bits_to_shift;
}

// Tail operations
inline std::int64_t
raw_tail_volatile(const AtomicBuffer &log_meta_data_buffer) {
  const std::int32_t partition_index =
      index_by_term_count(active_term_count(log_meta_data_buffer));
  std::size_t index =
      TERM_TAIL_COUNTERS_OFFSET + (partition_index * sizeof(std::int64_t));
  return log_meta_data_buffer.get_int64_volatile(index);
}

inline std::int64_t raw_tail_volatile(const AtomicBuffer &log_meta_data_buffer,
                                      int partition_index) {
  std::size_t index =
      TERM_TAIL_COUNTERS_OFFSET + (partition_index * sizeof(std::int64_t));
  return log_meta_data_buffer.get_int64_volatile(index);
}

inline std::int64_t raw_tail(const AtomicBuffer &log_meta_data_buffer) {
  const std::int32_t partition_index =
      index_by_term_count(active_term_count(log_meta_data_buffer));
  std::size_t index =
      TERM_TAIL_COUNTERS_OFFSET + (partition_index * sizeof(std::int64_t));
  return log_meta_data_buffer.get_int64(index);
}

inline std::int64_t raw_tail(const AtomicBuffer &log_meta_data_buffer,
                             int partition_index) {
  std::size_t index =
      TERM_TAIL_COUNTERS_OFFSET + (partition_index * sizeof(std::int64_t));
  return log_meta_data_buffer.get_int64(index);
}

inline std::int32_t term_id(const std::int64_t raw_tail) {
  return static_cast<std::int32_t>(raw_tail >> 32);
}

inline std::int32_t term_offset(const std::int64_t raw_tail,
                                const std::int64_t term_length) {
  const std::int64_t tail = raw_tail & 0xFFFFFFFFLL;
  const std::int64_t term_len = static_cast<std::int64_t>(term_length);
  if (tail < term_len) {
    return static_cast<std::int32_t>(tail);
  } else {
    return static_cast<std::int32_t>(term_len);
  }
}

inline bool cas_raw_tail(AtomicBuffer &log_meta_data_buffer,
                         int partition_index, std::int64_t expected_raw_tail,
                         std::int64_t update_raw_tail) {
  std::size_t index =
      TERM_TAIL_COUNTERS_OFFSET + (partition_index * sizeof(std::int64_t));
  return log_meta_data_buffer.compare_and_set_int64(index, expected_raw_tail,
                                                    update_raw_tail);
}

inline std::int32_t tail_counter_offset(int partition_index) {
  return static_cast<std::int32_t>(TERM_TAIL_COUNTERS_OFFSET +
                                   (partition_index * sizeof(std::int64_t)));
}

// Default frame header access
inline AtomicBuffer default_frame_header(AtomicBuffer &log_meta_data_buffer) {
  std::uint8_t *header =
      log_meta_data_buffer.buffer() + LOG_DEFAULT_FRAME_HEADER_OFFSET;
  return AtomicBuffer(header, 32); // DataFrameHeader::LENGTH equivalent
}

// Log rotation
inline void rotate_log(AtomicBuffer &log_meta_data_buffer,
                       std::int32_t current_term_count,
                       std::int32_t current_term_id) {
  const std::int32_t next_term_id = current_term_id + 1;
  const std::int32_t next_term_count = current_term_count + 1;
  const int next_index = index_by_term_count(next_term_count);
  const std::int32_t expected_term_id = next_term_id - PARTITION_COUNT;
  const std::int64_t new_raw_tail =
      (static_cast<std::int64_t>(next_term_id) << 32);

  std::int64_t raw_tail;
  do {
    raw_tail = raw_tail_volatile(log_meta_data_buffer, next_index);
    if (expected_term_id != term_id(raw_tail)) {
      break;
    }
  } while (
      !cas_raw_tail(log_meta_data_buffer, next_index, raw_tail, new_raw_tail));

  cas_active_term_count(log_meta_data_buffer, current_term_count,
                        next_term_count);
}

inline void initialize_tail_with_term_id(AtomicBuffer &log_meta_data_buffer,
                                         int partition_index,
                                         std::int32_t term_id) {
  const std::int64_t raw_tail = static_cast<std::int64_t>(term_id) << 32;
  std::size_t index =
      TERM_TAIL_COUNTERS_OFFSET + (partition_index * sizeof(std::int64_t));
  log_meta_data_buffer.put_int64(index, raw_tail);
}

// Position calculation utility
inline std::int32_t
position_bits_to_shift(const std::int32_t term_buffer_length) {
  switch (term_buffer_length) {
  case 64 * 1024:
    return 16;
  case 128 * 1024:
    return 17;
  case 256 * 1024:
    return 18;
  case 512 * 1024:
    return 19;
  case 1024 * 1024:
    return 20;
  case 2 * 1024 * 1024:
    return 21;
  case 4 * 1024 * 1024:
    return 22;
  case 8 * 1024 * 1024:
    return 23;
  case 16 * 1024 * 1024:
    return 24;
  case 32 * 1024 * 1024:
    return 25;
  case 64 * 1024 * 1024:
    return 26;
  case 128 * 1024 * 1024:
    return 27;
  case 256 * 1024 * 1024:
    return 28;
  case 512 * 1024 * 1024:
    return 29;
  case 1024 * 1024 * 1024:
    return 30;
  default:
    throw std::invalid_argument("invalid term buffer length: " +
                                std::to_string(term_buffer_length));
  }
}

// Fragmented frame length calculation
inline std::size_t
compute_fragmented_frame_length(const std::size_t length,
                                const std::size_t max_payload_length) {
  const int num_max_payloads = length / max_payload_length;
  const std::size_t remaining_payload = length % max_payload_length;
  const std::size_t last_frame_length =
      remaining_payload > 0
          ? util::BitUtil::align(remaining_payload + 32, 32)
          : 0; // 32 = DataFrameHeader::LENGTH + FRAME_ALIGNMENT

  return (num_max_payloads * (max_payload_length + 32)) + last_frame_length;
}

} // namespace LogBufferDescriptor

} // namespace logbuffer
} // namespace concurrent
} // namespace aeron
