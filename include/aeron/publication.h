#pragma once

#include "concurrent/atomic_buffer.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

namespace aeron {

/**
 * Aeron publication for sending messages to a single stream.
 * Based on the original Aeron Java implementation.
 */
class Publication {
public:
  /**
   * Result of an offer operation.
   */
  enum class OfferResult : std::int64_t {
    SUCCESS = 1,
    BACK_PRESSURE = -1,
    NOT_CONNECTED = -2,
    ADMIN_ACTION = -3,
    CLOSED = -4,
    MAX_POSITION_EXCEEDED = -5
  };

  /**
   * Constructor for Publication.
   */
  Publication(const std::string &channel, std::int32_t stream_id,
              std::int64_t registration_id,
              concurrent::AtomicBuffer log_meta_data_buffer,
              std::array<concurrent::AtomicBuffer, 3> term_buffers,
              std::int32_t position_bits_to_shift, std::int32_t initial_term_id,
              std::int32_t mtu_length) noexcept;

  /**
   * Simplified constructor for basic usage.
   */
  Publication(const std::string &channel, std::int32_t stream_id)
      : channel_(channel), stream_id_(stream_id), registration_id_(1),
        position_bits_to_shift_(16), initial_term_id_(0), mtu_length_(1408),
        is_closed_(false) {
    // Create dummy buffers for simplified implementation
  }

  /**
   * Non-blocking offer of a message to the stream.
   */
  OfferResult offer(const std::uint8_t *buffer, std::size_t length) noexcept {
    // Simplified implementation
    if (is_closed_.load()) {
      return OfferResult::CLOSED;
    }
    // For now, just return success
    return OfferResult::SUCCESS;
  }

  /**
   * Non-blocking offer with reserved value header.
   */
  OfferResult offer(const std::uint8_t *buffer, std::size_t length,
                    std::int64_t reserved_value) noexcept;

  /**
   * Try to claim a range in the publication log to write message(s) into.
   */
  std::int64_t try_claim(std::size_t length,
                         concurrent::AtomicBuffer &buffer_claim) noexcept {
    // Simplified implementation
    if (is_closed_.load()) {
      return static_cast<std::int64_t>(OfferResult::CLOSED);
    }
    // For now, just return a dummy position
    return 1;
  }

  /**
   * Commit the message that has been claimed.
   */
  void commit() noexcept;

  /**
   * Abort a claim that has been made but not committed.
   */
  void abort() noexcept;

  /**
   * Get the current position of the publication.
   */
  std::int64_t position() const noexcept;

  /**
   * Get the position limit beyond which the publication will not progress.
   */
  std::int64_t position_limit() const noexcept;

  /**
   * Get the stream identifier for this publication.
   */
  std::int32_t stream_id() const noexcept { return stream_id_; }

  /**
   * Get the channel for this publication.
   */
  const std::string &channel() const noexcept { return channel_; }

  /**
   * Get the registration id for this publication.
   */
  std::int64_t registration_id() const noexcept { return registration_id_; }

  /**
   * Check if this publication is closed.
   */
  bool is_closed() const noexcept { return is_closed_.load(); }

  /**
   * Check if this publication is connected to the media driver.
   */
  bool is_connected() const noexcept { return !is_closed_.load(); }

  /**
   * Get the maximum possible position this stream can reach.
   */
  std::int64_t max_possible_position() const noexcept {
    return max_possible_position_;
  }

  /**
   * Close this publication.
   */
  void close() noexcept;

  /**
   * Get the original channel passed when this publication was created.
   */
  const std::string &original_channel() const noexcept { return channel_; }

  /**
   * Get the maximum message length for this publication.
   */
  std::int32_t max_message_length() const noexcept {
    return mtu_length_ - HEADER_LENGTH;
  }

  /**
   * Get the maximum payload length for this publication.
   */
  std::int32_t max_payload_length() const noexcept {
    return mtu_length_ - HEADER_LENGTH;
  }

private:
  // Constants
  static constexpr std::int32_t HEADER_LENGTH = 32;
  static constexpr std::int32_t SEND_SETUP_FLAG = 0x20;
  static constexpr std::int64_t BACK_PRESSURE_STATUS = -1;
  static constexpr std::int64_t NOT_CONNECTED_STATUS = -2;
  static constexpr std::int64_t ADMIN_ACTION_STATUS = -3;
  static constexpr std::int64_t CLOSED_STATUS = -4;
  static constexpr std::int64_t MAX_POSITION_EXCEEDED_STATUS = -5;

  // Member variables
  std::string channel_;
  std::int32_t stream_id_;
  std::int64_t registration_id_;
  concurrent::AtomicBuffer log_meta_data_buffer_;
  std::array<concurrent::AtomicBuffer, 3> term_buffers_;
  std::int32_t position_bits_to_shift_;
  std::int32_t initial_term_id_;
  std::int32_t mtu_length_;
  std::int64_t max_possible_position_;
  std::int32_t max_message_length_;
  std::int32_t max_payload_length_;
  std::int32_t active_partition_index_;
  std::int32_t term_length_mask_;
  std::atomic<bool> is_closed_;

  // Claimed state
  std::int64_t active_term_id_;
  concurrent::AtomicBuffer active_term_buffer_;
  concurrent::AtomicBuffer header_writer_;

  // Helper methods
  OfferResult back_pressure_status(std::int64_t current_position,
                                   std::int32_t message_length) noexcept;

  std::int64_t new_position(std::int32_t result_offset) noexcept;

  void check_for_max_position_exceeded(std::int64_t position) const;

  std::int32_t
  append_unfragmented_message(std::int32_t term_id, std::int32_t term_offset,
                              concurrent::AtomicBuffer &term_buffer,
                              const std::uint8_t *buffer, std::size_t length,
                              std::int64_t reserved_value) noexcept;

  std::int32_t append_fragmented_message(std::int32_t term_id,
                                         std::int32_t term_offset,
                                         concurrent::AtomicBuffer &term_buffer,
                                         const std::uint8_t *buffer,
                                         std::size_t length,
                                         std::int64_t reserved_value) noexcept;

  void header_write(concurrent::AtomicBuffer &term_buffer,
                    std::int32_t term_offset, std::size_t length,
                    std::int32_t term_id, std::int64_t reserved_value) noexcept;

  void rotate_term() noexcept;

  std::int64_t get_and_add_raw_tail(std::int32_t partition_index,
                                    std::int32_t delta) noexcept;
};

/**
 * Buffer claim for zero-copy writing into a publication.
 */
class BufferClaim {
public:
  /**
   * Wrap a buffer for claiming.
   */
  void wrap(concurrent::AtomicBuffer &buffer, std::int32_t offset,
            std::int32_t length) noexcept {
    buffer_ = buffer;
    offset_ = offset;
    length_ = length;
  }

  /**
   * Get the buffer to write into.
   */
  concurrent::AtomicBuffer &buffer() noexcept { return buffer_; }

  /**
   * Get the offset within the buffer.
   */
  std::int32_t offset() const noexcept { return offset_; }

  /**
   * Get the length of the claim.
   */
  std::int32_t length() const noexcept { return length_; }

  /**
   * Put bytes into the claimed buffer.
   */
  void put_bytes(std::int32_t index, const std::uint8_t *src,
                 std::size_t length) noexcept {
    buffer_.put_bytes(offset_ + index, src, length);
  }

  /**
   * Commit this buffer claim.
   */
  void commit() noexcept { buffer_.put_int32_ordered(offset_, length_); }

  /**
   * Abort this buffer claim.
   */
  void abort() noexcept { buffer_.put_int32_ordered(offset_, 0); }

private:
  concurrent::AtomicBuffer buffer_;
  std::int32_t offset_;
  std::int32_t length_;
};

/**
 * Exclusive publication that can only be used by one thread at a time.
 */
class ExclusivePublication : public Publication {
public:
  /**
   * Constructor for ExclusivePublication.
   */
  ExclusivePublication(const std::string &channel, std::int32_t stream_id,
                       std::int64_t registration_id,
                       concurrent::AtomicBuffer log_meta_data_buffer,
                       std::array<concurrent::AtomicBuffer, 3> term_buffers,
                       std::int32_t position_bits_to_shift,
                       std::int32_t initial_term_id,
                       std::int32_t mtu_length) noexcept;

  /**
   * Non-blocking offer with exclusive access guarantee.
   */
  OfferResult offer_exclusive(const std::uint8_t *buffer,
                              std::size_t length) noexcept;

  /**
   * Try to claim with exclusive access guarantee.
   */
  std::int64_t try_claim_exclusive(std::size_t length,
                                   BufferClaim &buffer_claim) noexcept;
};

} // namespace aeron
