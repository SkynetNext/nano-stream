#pragma once

#include <cstdint>
#include <cstring>

namespace aeron {
namespace protocol {

/**
 * Base header for all Aeron protocol messages.
 */
struct Header {
  std::int32_t frame_length;
  std::uint8_t version;
  std::uint8_t flags;
  std::uint8_t type;
  std::uint8_t reserved;
  std::int32_t term_offset;
  std::int32_t session_id;
  std::int32_t stream_id;
  std::int32_t term_id;
  std::int64_t reserved_value;

  static constexpr std::int32_t HEADER_LENGTH = 32;
  static constexpr std::uint8_t CURRENT_VERSION = 1;

  void init() {
    std::memset(this, 0, sizeof(Header));
    version = CURRENT_VERSION;
  }

  void set_frame_length(std::int32_t length) { frame_length = length; }

  std::int32_t get_frame_length() const { return frame_length; }

  void set_flags(std::uint8_t flags) { this->flags = flags; }

  std::uint8_t get_flags() const { return flags; }

  void set_type(std::uint8_t type) { this->type = type; }

  std::uint8_t get_type() const { return type; }
};

/**
 * Data header for data messages.
 */
struct DataHeader : public Header {
  static constexpr std::uint8_t HDR_TYPE_DATA = 0x01;
  static constexpr std::uint8_t BEGIN_FRAG_FLAG = 0x80;
  static constexpr std::uint8_t END_FRAG_FLAG = 0x40;
  static constexpr std::uint8_t UNFRAGMENTED = BEGIN_FRAG_FLAG | END_FRAG_FLAG;

  void init() {
    Header::init();
    type = HDR_TYPE_DATA;
  }

  void set_begin_fragment() { flags |= BEGIN_FRAG_FLAG; }

  void set_end_fragment() { flags |= END_FRAG_FLAG; }

  void set_unfragmented() { flags = UNFRAGMENTED; }

  bool is_begin_fragment() const { return (flags & BEGIN_FRAG_FLAG) != 0; }

  bool is_end_fragment() const { return (flags & END_FRAG_FLAG) != 0; }

  bool is_fragmented() const { return flags != UNFRAGMENTED; }
};

/**
 * Setup header for connection setup messages.
 */
struct SetupHeader : public Header {
  static constexpr std::uint8_t HDR_TYPE_SETUP = 0x05;

  std::int32_t initial_term_id;
  std::int32_t active_term_id;
  std::int32_t term_offset;
  std::int32_t term_length;
  std::int32_t mtu_length;
  std::int32_t ttl;
  std::int32_t flow_control_timeout;
  std::int32_t session_id;
  std::int32_t stream_id;

  void init() {
    Header::init();
    type = HDR_TYPE_SETUP;
  }
};

/**
 * NAK header for negative acknowledgments.
 */
struct NakHeader : public Header {
  static constexpr std::uint8_t HDR_TYPE_NAK = 0x02;

  std::int32_t session_id;
  std::int32_t stream_id;
  std::int32_t term_id;
  std::int32_t term_offset;
  std::int32_t length;

  void init() {
    Header::init();
    type = HDR_TYPE_NAK;
  }
};

/**
 * Status Message header for flow control.
 */
struct StatusMessageHeader : public Header {
  static constexpr std::uint8_t HDR_TYPE_SM = 0x03;

  std::int32_t session_id;
  std::int32_t stream_id;
  std::int32_t consumption_term_id;
  std::int32_t consumption_term_offset;
  std::int32_t receiver_window_length;
  std::int32_t receiver_id;

  void init() {
    Header::init();
    type = HDR_TYPE_SM;
  }
};

} // namespace protocol
} // namespace aeron
