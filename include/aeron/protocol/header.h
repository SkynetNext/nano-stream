#pragma once

#include <cstdint>
#include <cstring>

namespace aeron {
namespace protocol {

/**
 * Aeron message header format.
 * Compatible with Java Aeron protocol.
 */
struct alignas(32) DataHeader {
  static constexpr std::int32_t HEADER_LENGTH = 32;
  static constexpr std::int32_t DATA_OFFSET = HEADER_LENGTH;

  // Header fields
  std::int32_t frame_length;   // Total frame length including header
  std::int8_t version;         // Protocol version
  std::int8_t flags;           // Frame flags
  std::int16_t type;           // Frame type
  std::int32_t term_offset;    // Offset within term
  std::int32_t session_id;     // Session identifier
  std::int32_t stream_id;      // Stream identifier
  std::int32_t term_id;        // Term identifier
  std::int64_t reserved_value; // Reserved for future use

  /**
   * Initialize header with default values.
   */
  void init() {
    std::memset(this, 0, sizeof(*this));
    version = 1;
  }

  /**
   * Set frame length including header.
   */
  void set_frame_length(std::int32_t length) { frame_length = length; }

  /**
   * Get the data length (excluding header).
   */
  std::int32_t data_length() const { return frame_length - HEADER_LENGTH; }

  /**
   * Check if this is the beginning of a message.
   */
  bool is_begin_fragment() const { return (flags & BEGIN_FRAGMENT_FLAG) != 0; }

  /**
   * Check if this is the end of a message.
   */
  bool is_end_fragment() const { return (flags & END_FRAGMENT_FLAG) != 0; }

  /**
   * Mark as beginning fragment.
   */
  void set_begin_fragment() { flags |= BEGIN_FRAGMENT_FLAG; }

  /**
   * Mark as end fragment.
   */
  void set_end_fragment() { flags |= END_FRAGMENT_FLAG; }

private:
  static constexpr std::int8_t BEGIN_FRAGMENT_FLAG = 0x80;
  static constexpr std::int8_t END_FRAGMENT_FLAG = 0x40;
};

/**
 * Setup frame for initial connection.
 */
struct SetupFrame {
  DataHeader header;
  std::int32_t term_length;
  std::int32_t mtu_length;
  std::int32_t initial_term_id;
  std::int32_t active_term_id;
  std::int32_t term_offset;

  void init(std::int32_t session_id, std::int32_t stream_id) {
    header.init();
    header.session_id = session_id;
    header.stream_id = stream_id;
    header.type = SETUP_FRAME_TYPE;
    header.set_frame_length(sizeof(*this));
  }

private:
  static constexpr std::int16_t SETUP_FRAME_TYPE = 0x01;
};

/**
 * Status message frame.
 */
struct StatusMessageFrame {
  DataHeader header;
  std::int32_t consumption_term_id;
  std::int32_t consumption_term_offset;
  std::int32_t receiver_window_length;

  void init(std::int32_t session_id, std::int32_t stream_id) {
    header.init();
    header.session_id = session_id;
    header.stream_id = stream_id;
    header.type = STATUS_MESSAGE_FRAME_TYPE;
    header.set_frame_length(sizeof(*this));
  }

private:
  static constexpr std::int16_t STATUS_MESSAGE_FRAME_TYPE = 0x02;
};

} // namespace protocol
} // namespace aeron
