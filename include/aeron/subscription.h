#pragma once

#include "concurrent/atomic_buffer.h"
#include "util/bit_util.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace aeron {

/**
 * Fragment handler function type for processing received messages.
 */
using fragment_handler_t = std::function<void(
    const concurrent::AtomicBuffer &buffer, std::int32_t offset,
    std::int32_t length, const concurrent::AtomicBuffer &header)>;

/**
 * Controlled fragment handler that can indicate back pressure.
 */
enum class ControlledPollAction {
  /**
   * Continue processing fragments.
   */
  CONTINUE,
  /**
   * Abort current polling operation.
   */
  ABORT,
  /**
   * Break from current polling operation.
   */
  BREAK,
  /**
   * Commit position and continue.
   */
  COMMIT
};

using controlled_fragment_handler_t = std::function<ControlledPollAction(
    const concurrent::AtomicBuffer &buffer, std::int32_t offset,
    std::int32_t length, const concurrent::AtomicBuffer &header)>;

/**
 * Aeron subscription for receiving messages from a stream.
 * Based on the original Aeron Java implementation.
 */
class Subscription {
public:
  /**
   * Constructor for Subscription.
   */
  Subscription(const std::string &channel, std::int32_t stream_id,
               std::int64_t registration_id) noexcept;

  /**
   * Simplified constructor for basic usage.
   */
  Subscription(const std::string &channel, std::int32_t stream_id)
      : channel_(channel), stream_id_(stream_id), registration_id_(1),
        is_closed_(false) {
    // Simplified implementation
  }

  /**
   * Poll for new messages in a non-blocking fashion.
   */
  int poll(fragment_handler_t handler, int fragment_limit) noexcept {
    // Simplified implementation - no actual messages to poll
    return 0;
  }

  /**
   * Controlled poll that allows the handler to control the polling behavior.
   */
  int controlled_poll(controlled_fragment_handler_t handler,
                      int fragment_limit) noexcept;

  /**
   * Poll for new messages with a bounded length.
   */
  int bounded_poll(fragment_handler_t handler, std::int32_t offset,
                   std::int32_t length, int fragment_limit) noexcept;

  /**
   * Try to peek at the next message without advancing the position.
   */
  std::int64_t try_peek_next() noexcept;

  /**
   * Get the current position of this subscription.
   */
  std::int64_t position() const noexcept;

  /**
   * Seek to a given position in the stream.
   */
  void seek(std::int64_t position) noexcept;

  /**
   * Check if this subscription is connected to the publication.
   */
  bool is_connected() const noexcept;

  /**
   * Check if this subscription has any images.
   */
  bool has_images() const noexcept { return !images_.empty(); }

  /**
   * Get the number of images for this subscription.
   */
  std::size_t image_count() const noexcept { return images_.size(); }

  /**
   * Get the stream identifier for this subscription.
   */
  std::int32_t stream_id() const noexcept { return stream_id_; }

  /**
   * Get the channel for this subscription.
   */
  const std::string &channel() const noexcept { return channel_; }

  /**
   * Get the registration id for this subscription.
   */
  std::int64_t registration_id() const noexcept { return registration_id_; }

  /**
   * Check if this subscription is closed.
   */
  bool is_closed() const noexcept { return is_closed_.load(); }

  /**
   * Close this subscription.
   */
  void close() noexcept;

  /**
   * Add an available image notification handler.
   */
  void add_destination(const std::string &endpoint_channel) noexcept;

  /**
   * Remove a destination from this subscription.
   */
  void remove_destination(const std::string &endpoint_channel) noexcept;

  /**
   * Get the local socket addresses for this subscription.
   */
  std::vector<std::string> local_socket_addresses() const noexcept;

  /**
   * Get the original channel passed when this subscription was created.
   */
  const std::string &original_channel() const noexcept { return channel_; }

  /**
   * Get all the images associated with this subscription.
   */
  const std::vector<std::shared_ptr<class Image>> &images() const noexcept {
    return images_;
  }

  /**
   * Get the image associated with the given session id.
   */
  std::shared_ptr<class Image>
  image_by_session_id(std::int32_t session_id) const noexcept;

  /**
   * Iterate over all images for this subscription.
   */
  template <typename Function> void for_each_image(Function &&func) const {
    for (const auto &image : images_) {
      func(image);
    }
  }

private:
  std::string channel_;
  std::int32_t stream_id_;
  std::int64_t registration_id_;
  std::atomic<bool> is_closed_;
  std::vector<std::shared_ptr<class Image>> images_;
  std::int32_t round_robin_index_;

  // Internal methods
  void add_image(std::shared_ptr<class Image> image) noexcept;
  void remove_image(std::shared_ptr<class Image> image) noexcept;
  std::shared_ptr<class Image> next_image() noexcept;
};

/**
 * Image represents a replicated stream from a publisher.
 */
class Image {
public:
  /**
   * Constructor for Image.
   */
  Image(std::int32_t session_id, std::int64_t correlation_id,
        std::int64_t subscription_registration_id,
        const std::string &source_identity,
        concurrent::AtomicBuffer log_meta_data_buffer,
        std::array<concurrent::AtomicBuffer, 3> term_buffers,
        std::int32_t position_bits_to_shift, std::int32_t initial_term_id,
        std::int32_t term_length) noexcept;

  /**
   * Poll for new fragments in this image.
   */
  int poll(fragment_handler_t handler, int fragment_limit) noexcept;

  /**
   * Controlled poll for new fragments.
   */
  int controlled_poll(controlled_fragment_handler_t handler,
                      int fragment_limit) noexcept;

  /**
   * Bounded poll for new fragments.
   */
  int bounded_poll(fragment_handler_t handler, std::int64_t limit_position,
                   int fragment_limit) noexcept;

  /**
   * Get the current position of this image.
   */
  std::int64_t position() const noexcept;

  /**
   * Set the position for this image.
   */
  void position(std::int64_t new_position) noexcept;

  /**
   * Check if this image is closed.
   */
  bool is_closed() const noexcept { return is_closed_.load(); }

  /**
   * Check if this image is connected.
   */
  bool is_connected() const noexcept;

  /**
   * Get the session id for this image.
   */
  std::int32_t session_id() const noexcept { return session_id_; }

  /**
   * Get the correlation id for this image.
   */
  std::int64_t correlation_id() const noexcept { return correlation_id_; }

  /**
   * Get the source identity for this image.
   */
  const std::string &source_identity() const noexcept {
    return source_identity_;
  }

  /**
   * Get the term length for this image.
   */
  std::int32_t term_length() const noexcept { return term_length_; }

  /**
   * Get the MTU length for this image.
   */
  std::int32_t mtu_length() const noexcept;

  /**
   * Get the initial term id for this image.
   */
  std::int32_t initial_term_id() const noexcept { return initial_term_id_; }

  /**
   * Get the active term count for this image.
   */
  std::int32_t active_term_count() const noexcept;

  /**
   * Close this image.
   */
  void close() noexcept;

private:
  std::int32_t session_id_;
  std::int64_t correlation_id_;
  std::int64_t subscription_registration_id_;
  std::string source_identity_;
  concurrent::AtomicBuffer log_meta_data_buffer_;
  std::array<concurrent::AtomicBuffer, 3> term_buffers_;
  std::int32_t position_bits_to_shift_;
  std::int32_t initial_term_id_;
  std::int32_t term_length_;
  std::int32_t term_length_mask_;
  std::atomic<bool> is_closed_;
  std::atomic<std::int64_t> subscriber_position_;

  // Internal methods
  int read_messages(concurrent::AtomicBuffer &term_buffer,
                    std::int32_t term_offset, fragment_handler_t handler,
                    int fragment_limit, const concurrent::AtomicBuffer &header,
                    std::int64_t &position) noexcept;

  int controlled_read_messages(concurrent::AtomicBuffer &term_buffer,
                               std::int32_t term_offset,
                               controlled_fragment_handler_t handler,
                               int fragment_limit,
                               const concurrent::AtomicBuffer &header,
                               std::int64_t &position) noexcept;

  std::int32_t scan_for_availability(concurrent::AtomicBuffer &term_buffer,
                                     std::int32_t term_offset,
                                     std::int32_t mtu_length) noexcept;

  void validate_position(std::int64_t new_position) const;
};

/**
 * Header structure for Aeron frames.
 */
struct DataFrameHeader {
  std::int32_t frame_length;
  std::uint8_t version;
  std::uint8_t flags;
  std::uint16_t type;
  std::int32_t term_offset;
  std::int32_t session_id;
  std::int32_t stream_id;
  std::int32_t term_id;
  std::int64_t reserved_value;

  static constexpr std::int32_t LENGTH = 32;
  static constexpr std::uint16_t HDR_TYPE_DATA = 0x01;
  static constexpr std::uint16_t HDR_TYPE_PAD = 0x02;
  static constexpr std::uint8_t BEGIN_FLAG = 0x80;
  static constexpr std::uint8_t END_FLAG = 0x40;
  static constexpr std::uint8_t UNFRAGMENTED = BEGIN_FLAG | END_FLAG;
};

/**
 * Utility functions for frame handling.
 */
namespace frame_util {

/**
 * Get the frame length from a term buffer at the given offset.
 */
inline std::int32_t frame_length(const concurrent::AtomicBuffer &term_buffer,
                                 std::int32_t term_offset) noexcept {
  return term_buffer.get_int32_volatile(term_offset);
}

/**
 * Get the frame type from a term buffer at the given offset.
 */
inline std::uint16_t frame_type(const concurrent::AtomicBuffer &term_buffer,
                                std::int32_t term_offset) noexcept {
  return term_buffer.get_uint16(term_offset + 6);
}

/**
 * Check if a frame is padding.
 */
inline bool is_padding_frame(const concurrent::AtomicBuffer &term_buffer,
                             std::int32_t term_offset) noexcept {
  return frame_type(term_buffer, term_offset) == DataFrameHeader::HDR_TYPE_PAD;
}

/**
 * Align a frame length to the appropriate boundary.
 */
inline std::int32_t align_frame_length(std::int32_t length) noexcept {
  return util::BitUtil::align(length, 32);
}

} // namespace frame_util

} // namespace aeron
