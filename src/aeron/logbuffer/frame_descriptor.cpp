#include "aeron/logbuffer/frame_descriptor.h"

namespace aeron {
namespace logbuffer {

std::int32_t FrameDescriptor::frameLength(const std::uint8_t *buffer,
                                          std::int32_t offset) {
  return *reinterpret_cast<const std::int32_t *>(buffer + offset +
                                                 FRAME_LENGTH_FIELD_OFFSET);
}

void FrameDescriptor::frameLength(std::uint8_t *buffer, std::int32_t offset,
                                  std::int32_t length) {
  *reinterpret_cast<std::int32_t *>(buffer + offset +
                                    FRAME_LENGTH_FIELD_OFFSET) = length;
}

std::uint8_t FrameDescriptor::frameVersion(const std::uint8_t *buffer,
                                           std::int32_t offset) {
  return buffer[offset + VERSION_FIELD_OFFSET];
}

void FrameDescriptor::frameVersion(std::uint8_t *buffer, std::int32_t offset,
                                   std::uint8_t version) {
  buffer[offset + VERSION_FIELD_OFFSET] = version;
}

std::uint8_t FrameDescriptor::frameFlags(const std::uint8_t *buffer,
                                         std::int32_t offset) {
  return buffer[offset + FLAGS_FIELD_OFFSET];
}

void FrameDescriptor::frameFlags(std::uint8_t *buffer, std::int32_t offset,
                                 std::uint8_t flags) {
  buffer[offset + FLAGS_FIELD_OFFSET] = flags;
}

std::uint8_t FrameDescriptor::frameType(const std::uint8_t *buffer,
                                        std::int32_t offset) {
  return buffer[offset + TYPE_FIELD_OFFSET];
}

void FrameDescriptor::frameType(std::uint8_t *buffer, std::int32_t offset,
                                std::uint8_t type) {
  buffer[offset + TYPE_FIELD_OFFSET] = type;
}

std::int32_t FrameDescriptor::termOffset(const std::uint8_t *buffer,
                                         std::int32_t offset) {
  return *reinterpret_cast<const std::int32_t *>(buffer + offset +
                                                 TERM_OFFSET_FIELD_OFFSET);
}

void FrameDescriptor::termOffset(std::uint8_t *buffer, std::int32_t offset,
                                 std::int32_t termOffset) {
  *reinterpret_cast<std::int32_t *>(buffer + offset +
                                    TERM_OFFSET_FIELD_OFFSET) = termOffset;
}

std::int32_t FrameDescriptor::sessionId(const std::uint8_t *buffer,
                                        std::int32_t offset) {
  return *reinterpret_cast<const std::int32_t *>(buffer + offset +
                                                 SESSION_ID_FIELD_OFFSET);
}

void FrameDescriptor::sessionId(std::uint8_t *buffer, std::int32_t offset,
                                std::int32_t sessionId) {
  *reinterpret_cast<std::int32_t *>(buffer + offset + SESSION_ID_FIELD_OFFSET) =
      sessionId;
}

std::int32_t FrameDescriptor::streamId(const std::uint8_t *buffer,
                                       std::int32_t offset) {
  return *reinterpret_cast<const std::int32_t *>(buffer + offset +
                                                 STREAM_ID_FIELD_OFFSET);
}

void FrameDescriptor::streamId(std::uint8_t *buffer, std::int32_t offset,
                               std::int32_t streamId) {
  *reinterpret_cast<std::int32_t *>(buffer + offset + STREAM_ID_FIELD_OFFSET) =
      streamId;
}

std::int32_t FrameDescriptor::termId(const std::uint8_t *buffer,
                                     std::int32_t offset) {
  return *reinterpret_cast<const std::int32_t *>(buffer + offset +
                                                 TERM_ID_FIELD_OFFSET);
}

void FrameDescriptor::termId(std::uint8_t *buffer, std::int32_t offset,
                             std::int32_t termId) {
  *reinterpret_cast<std::int32_t *>(buffer + offset + TERM_ID_FIELD_OFFSET) =
      termId;
}

std::int64_t FrameDescriptor::reservedValue(const std::uint8_t *buffer,
                                            std::int32_t offset) {
  return *reinterpret_cast<const std::int64_t *>(buffer + offset +
                                                 RESERVED_VALUE_FIELD_OFFSET);
}

void FrameDescriptor::reservedValue(std::uint8_t *buffer, std::int32_t offset,
                                    std::int64_t value) {
  *reinterpret_cast<std::int64_t *>(buffer + offset +
                                    RESERVED_VALUE_FIELD_OFFSET) = value;
}

bool FrameDescriptor::isDataFrame(const std::uint8_t *buffer,
                                  std::int32_t offset) {
  return frameType(buffer, offset) == HDR_TYPE_DATA;
}

bool FrameDescriptor::isPaddingFrame(const std::uint8_t *buffer,
                                     std::int32_t offset) {
  return frameType(buffer, offset) == HDR_TYPE_PAD;
}

bool FrameDescriptor::isFragmented(const std::uint8_t *buffer,
                                   std::int32_t offset) {
  const std::uint8_t flags = frameFlags(buffer, offset);
  return (flags & (BEGIN_FRAG_FLAG | END_FRAG_FLAG)) != UNFRAGMENTED;
}

bool FrameDescriptor::isBeginFragment(const std::uint8_t *buffer,
                                      std::int32_t offset) {
  return (frameFlags(buffer, offset) & BEGIN_FRAG_FLAG) != 0;
}

bool FrameDescriptor::isEndFragment(const std::uint8_t *buffer,
                                    std::int32_t offset) {
  return (frameFlags(buffer, offset) & END_FRAG_FLAG) != 0;
}

bool FrameDescriptor::isUnfragmented(const std::uint8_t *buffer,
                                     std::int32_t offset) {
  return frameFlags(buffer, offset) == UNFRAGMENTED;
}

std::int32_t FrameDescriptor::align(std::int32_t value,
                                    std::int32_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

} // namespace logbuffer
} // namespace aeron
