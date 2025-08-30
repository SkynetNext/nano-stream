#pragma once

#include <cstdint>

namespace aeron {
namespace logbuffer {

/**
 * Description of the structure for message framing in a log buffer.
 *
 * Messages are written with a header that contains:
 * - Frame length
 * - Version
 * - Flags
 * - Type
 * - Term offset
 * - Session ID
 * - Stream ID
 * - Term ID
 * - Reserved value
 */
class FrameDescriptor {
public:
  static constexpr int FRAME_ALIGNMENT = 32;
  static constexpr int HEADER_LENGTH = 32;

  // Frame header field offsets
  static constexpr int FRAME_LENGTH_FIELD_OFFSET = 0;
  static constexpr int VERSION_FIELD_OFFSET = 4;
  static constexpr int FLAGS_FIELD_OFFSET = 5;
  static constexpr int TYPE_FIELD_OFFSET = 6;
  static constexpr int TERM_OFFSET_FIELD_OFFSET = 8;
  static constexpr int SESSION_ID_FIELD_OFFSET = 12;
  static constexpr int STREAM_ID_FIELD_OFFSET = 16;
  static constexpr int TERM_ID_FIELD_OFFSET = 20;
  static constexpr int RESERVED_VALUE_FIELD_OFFSET = 24;

  // Frame types
  static constexpr std::uint8_t HDR_TYPE_PAD = 0x00;
  static constexpr std::uint8_t HDR_TYPE_DATA = 0x01;
  static constexpr std::uint8_t HDR_TYPE_NAK = 0x02;
  static constexpr std::uint8_t HDR_TYPE_SM = 0x03;
  static constexpr std::uint8_t HDR_TYPE_ERR = 0x04;
  static constexpr std::uint8_t HDR_TYPE_SETUP = 0x05;
  static constexpr std::uint8_t HDR_TYPE_EXT = 0x06;
  static constexpr std::uint8_t HDR_TYPE_RTTM = 0x07;
  static constexpr std::uint8_t HDR_TYPE_RES = 0x08;

  // Frame flags
  static constexpr std::uint8_t BEGIN_FRAG_FLAG = 0x80;
  static constexpr std::uint8_t END_FRAG_FLAG = 0x40;
  static constexpr std::uint8_t UNFRAGMENTED = BEGIN_FRAG_FLAG | END_FRAG_FLAG;

  /**
   * Get the frame length from the header.
   */
  static std::int32_t frameLength(const std::uint8_t *buffer,
                                  std::int32_t offset);

  /**
   * Set the frame length in the header.
   */
  static void frameLength(std::uint8_t *buffer, std::int32_t offset,
                          std::int32_t length);

  /**
   * Get the frame version from the header.
   */
  static std::uint8_t frameVersion(const std::uint8_t *buffer,
                                   std::int32_t offset);

  /**
   * Set the frame version in the header.
   */
  static void frameVersion(std::uint8_t *buffer, std::int32_t offset,
                           std::uint8_t version);

  /**
   * Get the frame flags from the header.
   */
  static std::uint8_t frameFlags(const std::uint8_t *buffer,
                                 std::int32_t offset);

  /**
   * Set the frame flags in the header.
   */
  static void frameFlags(std::uint8_t *buffer, std::int32_t offset,
                         std::uint8_t flags);

  /**
   * Get the frame type from the header.
   */
  static std::uint8_t frameType(const std::uint8_t *buffer,
                                std::int32_t offset);

  /**
   * Set the frame type in the header.
   */
  static void frameType(std::uint8_t *buffer, std::int32_t offset,
                        std::uint8_t type);

  /**
   * Get the term offset from the header.
   */
  static std::int32_t termOffset(const std::uint8_t *buffer,
                                 std::int32_t offset);

  /**
   * Set the term offset in the header.
   */
  static void termOffset(std::uint8_t *buffer, std::int32_t offset,
                         std::int32_t termOffset);

  /**
   * Get the session ID from the header.
   */
  static std::int32_t sessionId(const std::uint8_t *buffer,
                                std::int32_t offset);

  /**
   * Set the session ID in the header.
   */
  static void sessionId(std::uint8_t *buffer, std::int32_t offset,
                        std::int32_t sessionId);

  /**
   * Get the stream ID from the header.
   */
  static std::int32_t streamId(const std::uint8_t *buffer, std::int32_t offset);

  /**
   * Set the stream ID in the header.
   */
  static void streamId(std::uint8_t *buffer, std::int32_t offset,
                       std::int32_t streamId);

  /**
   * Get the term ID from the header.
   */
  static std::int32_t termId(const std::uint8_t *buffer, std::int32_t offset);

  /**
   * Set the term ID in the header.
   */
  static void termId(std::uint8_t *buffer, std::int32_t offset,
                     std::int32_t termId);

  /**
   * Get the reserved value from the header.
   */
  static std::int64_t reservedValue(const std::uint8_t *buffer,
                                    std::int32_t offset);

  /**
   * Set the reserved value in the header.
   */
  static void reservedValue(std::uint8_t *buffer, std::int32_t offset,
                            std::int64_t value);

  /**
   * Check if the frame is a data frame.
   */
  static bool isDataFrame(const std::uint8_t *buffer, std::int32_t offset);

  /**
   * Check if the frame is a padding frame.
   */
  static bool isPaddingFrame(const std::uint8_t *buffer, std::int32_t offset);

  /**
   * Check if the frame is fragmented.
   */
  static bool isFragmented(const std::uint8_t *buffer, std::int32_t offset);

  /**
   * Check if the frame is the beginning of a fragmented message.
   */
  static bool isBeginFragment(const std::uint8_t *buffer, std::int32_t offset);

  /**
   * Check if the frame is the end of a fragmented message.
   */
  static bool isEndFragment(const std::uint8_t *buffer, std::int32_t offset);

  /**
   * Check if the frame is unfragmented.
   */
  static bool isUnfragmented(const std::uint8_t *buffer, std::int32_t offset);
};

} // namespace logbuffer
} // namespace aeron
