#include "aeron/client/publication.h"
#include "aeron/logbuffer/frame_descriptor.h"
#include <cstring>

namespace aeron {
namespace client {

PublicationBase::PublicationBase(const std::string &channel,
                                 std::int32_t stream_id,
                                 std::int32_t session_id,
                                 std::int64_t registration_id)
    : channel_(channel), stream_id_(stream_id), session_id_(session_id),
      registration_id_(registration_id) {}

void PublicationBase::close() { closed_.store(true); }

Publication::Publication(const std::string &channel, std::int32_t stream_id,
                         std::int32_t session_id, std::int64_t registration_id,
                         std::shared_ptr<util::MemoryMappedFile> log_buffer)
    : PublicationBase(channel, stream_id, session_id, registration_id),
      log_buffer_(log_buffer), term_length_(DEFAULT_TERM_LENGTH),
      max_message_length_(MAX_MESSAGE_LENGTH),
      max_payload_length_(term_length_ / 8) {

  // Initialize position limit to allow initial publishing
  position_limit_.store(term_length_);
}

PublicationResult Publication::offer(const std::uint8_t *buffer,
                                     std::size_t offset, std::size_t length) {
  if (closed_.load()) {
    return PublicationResult::CLOSED;
  }

  if (!is_connected()) {
    return PublicationResult::NOT_CONNECTED;
  }

  if (length > max_payload_length_) {
    return PublicationResult::MAX_POSITION_EXCEEDED;
  }

  std::int64_t current_position = position_.load();
  std::int64_t limit = position_limit_.load();

  if (current_position + static_cast<std::int64_t>(length) > limit) {
    return PublicationResult::BACK_PRESSURED;
  }

  // Write the data to the log buffer at the current position
  std::uint8_t *termBuffer =
      log_buffer_->memory() + (current_position % term_length_);
  std::int32_t termOffset =
      static_cast<std::int32_t>(current_position & (term_length_ - 1));

  // Check if we need to wrap to the next term
  if (termOffset + static_cast<std::int32_t>(length) > term_length_) {
    return PublicationResult::BACK_PRESSURED;
  }

  // Write frame header
  logbuffer::FrameDescriptor::frameLength(
      termBuffer, termOffset,
      static_cast<std::int32_t>(length +
                                logbuffer::FrameDescriptor::HEADER_LENGTH));
  logbuffer::FrameDescriptor::frameVersion(termBuffer, termOffset, 1);
  logbuffer::FrameDescriptor::frameFlags(
      termBuffer, termOffset, logbuffer::FrameDescriptor::UNFRAGMENTED);
  logbuffer::FrameDescriptor::frameType(
      termBuffer, termOffset, logbuffer::FrameDescriptor::HDR_TYPE_DATA);
  logbuffer::FrameDescriptor::termOffset(termBuffer, termOffset, termOffset);
  logbuffer::FrameDescriptor::sessionId(termBuffer, termOffset, session_id_);
  logbuffer::FrameDescriptor::streamId(termBuffer, termOffset, stream_id_);
  logbuffer::FrameDescriptor::termId(
      termBuffer, termOffset,
      static_cast<std::int32_t>(current_position / term_length_));
  logbuffer::FrameDescriptor::reservedValue(termBuffer, termOffset, 0);

  // Write data
  std::memcpy(termBuffer + termOffset +
                  logbuffer::FrameDescriptor::HEADER_LENGTH,
              buffer + offset, length);

  // Update position atomically
  position_.fetch_add(static_cast<std::int64_t>(
      length + logbuffer::FrameDescriptor::HEADER_LENGTH));

  return PublicationResult::SUCCESS;
}

std::int64_t Publication::try_claim(std::size_t length) {
  if (closed_.load()) {
    return static_cast<std::int64_t>(PublicationResult::CLOSED);
  }

  if (!is_connected()) {
    return static_cast<std::int64_t>(PublicationResult::NOT_CONNECTED);
  }

  if (length > max_payload_length_) {
    return static_cast<std::int64_t>(PublicationResult::MAX_POSITION_EXCEEDED);
  }

  std::int64_t current_position = position_.load();
  std::int64_t limit = position_limit_.load();

  if (current_position + static_cast<std::int64_t>(length) > limit) {
    return static_cast<std::int64_t>(PublicationResult::BACK_PRESSURED);
  }

  // Try to claim the space atomically
  std::int64_t expected = current_position;
  std::int64_t desired = current_position + static_cast<std::int64_t>(length);

  if (position_.compare_exchange_weak(expected, desired)) {
    return current_position; // Return the claimed position
  }

  return static_cast<std::int64_t>(PublicationResult::BACK_PRESSURED);
}

void Publication::commit(std::int64_t position) {
  // In a real implementation, this would mark the claimed space as committed
  // and make it available for reading by subscribers

  // For now, this is a no-op since we don't have actual log buffer management
}

void Publication::abort(std::int64_t position) {
  // In a real implementation, this would mark the claimed space as aborted
  // and make it available for reuse

  // For now, this is a no-op
}

bool Publication::is_connected() const {
  // In a real implementation, this would check the connection status with the
  // media driver For now, assume always connected if not closed
  return !closed_.load();
}

std::int64_t Publication::position() const { return position_.load(); }

std::int64_t Publication::position_limit() const {
  return position_limit_.load();
}

std::int64_t Publication::max_possible_position() const {
  return static_cast<std::int64_t>(term_length_) * 3; // 3 terms
}

ExclusivePublication::ExclusivePublication(
    const std::string &channel, std::int32_t stream_id, std::int32_t session_id,
    std::int64_t registration_id,
    std::shared_ptr<util::MemoryMappedFile> log_buffer)
    : PublicationBase(channel, stream_id, session_id, registration_id),
      log_buffer_(log_buffer), term_length_(DEFAULT_TERM_LENGTH),
      max_message_length_(MAX_MESSAGE_LENGTH),
      max_payload_length_(term_length_ / 8) {

  // Initialize position limit to allow initial publishing
  position_limit_.store(term_length_);
}

PublicationResult ExclusivePublication::offer(const std::uint8_t *buffer,
                                              std::size_t offset,
                                              std::size_t length) {
  if (closed_.load()) {
    return PublicationResult::CLOSED;
  }

  if (!is_connected()) {
    return PublicationResult::NOT_CONNECTED;
  }

  if (length > max_payload_length_) {
    return PublicationResult::MAX_POSITION_EXCEEDED;
  }

  std::int64_t limit = position_limit_.load();

  if (position_ + static_cast<std::int64_t>(length) > limit) {
    return PublicationResult::BACK_PRESSURED;
  }

  // In a real implementation, this would:
  // 1. Write the data to the log buffer at the current position
  // 2. Update the position (no atomics needed for exclusive access)
  // 3. Signal the media driver that new data is available

  // For now, just simulate successful publication
  position_ += static_cast<std::int64_t>(length);

  return PublicationResult::SUCCESS;
}

std::int64_t ExclusivePublication::try_claim(std::size_t length) {
  if (closed_.load()) {
    return static_cast<std::int64_t>(PublicationResult::CLOSED);
  }

  if (!is_connected()) {
    return static_cast<std::int64_t>(PublicationResult::NOT_CONNECTED);
  }

  if (length > max_payload_length_) {
    return static_cast<std::int64_t>(PublicationResult::MAX_POSITION_EXCEEDED);
  }

  std::int64_t limit = position_limit_.load();

  if (position_ + static_cast<std::int64_t>(length) > limit) {
    return static_cast<std::int64_t>(PublicationResult::BACK_PRESSURED);
  }

  std::int64_t claimed_position = position_;
  position_ += static_cast<std::int64_t>(length);

  return claimed_position;
}

void ExclusivePublication::commit(std::int64_t position) {
  // In a real implementation, this would mark the claimed space as committed
  // For exclusive publications, this is typically a no-op since there's no
  // contention
}

void ExclusivePublication::abort(std::int64_t position) {
  // In a real implementation, this would mark the claimed space as aborted
  // For exclusive publications, this might involve rolling back the position
}

bool ExclusivePublication::is_connected() const {
  // In a real implementation, this would check the connection status with the
  // media driver For now, assume always connected if not closed
  return !closed_.load();
}

std::int64_t ExclusivePublication::position() const { return position_; }

std::int64_t ExclusivePublication::position_limit() const {
  return position_limit_.load();
}

std::int64_t ExclusivePublication::max_possible_position() const {
  return static_cast<std::int64_t>(term_length_) * 3; // 3 terms
}

} // namespace client
} // namespace aeron
