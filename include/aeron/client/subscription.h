#pragma once

#include "../util/memory_mapped_file.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace aeron {
namespace client {

/**
 * Fragment handler function type.
 * Called for each message fragment received.
 *
 * @param buffer Message data
 * @param offset Offset in buffer
 * @param length Length of message
 * @param header Message header information
 */
using fragment_handler_t =
    std::function<void(const std::uint8_t *buffer, std::size_t offset,
                       std::size_t length, const void *header)>;

/**
 * Controlled fragment handler function type.
 * Called for each message fragment received, with flow control.
 *
 * @param buffer Message data
 * @param offset Offset in buffer
 * @param length Length of message
 * @param header Message header information
 * @return Action to take (CONTINUE, BREAK, ABORT, COMMIT)
 */
enum class ControlledPollAction {
  CONTINUE, // Continue processing
  BREAK,    // Stop processing this poll
  ABORT,    // Abort current message
  COMMIT    // Commit position and continue
};

using controlled_fragment_handler_t = std::function<ControlledPollAction(
    const std::uint8_t *buffer, std::size_t offset, std::size_t length,
    const void *header)>;

/**
 * Image represents a stream of messages from a single publisher.
 */
class Image {
public:
  /**
   * Constructor.
   */
  Image(std::int32_t session_id, std::int64_t correlation_id,
        std::shared_ptr<util::MemoryMappedFile> log_buffer);

  /**
   * Destructor.
   */
  ~Image();

  /**
   * Poll for new messages.
   *
   * @param handler Function to call for each message
   * @param fragment_limit Maximum number of fragments to process
   * @return Number of fragments processed
   */
  int poll(fragment_handler_t handler, int fragment_limit);

  /**
   * Controlled poll for new messages.
   *
   * @param handler Function to call for each message
   * @param fragment_limit Maximum number of fragments to process
   * @return Number of fragments processed
   */
  int controlled_poll(controlled_fragment_handler_t handler,
                      int fragment_limit);

  /**
   * Get the session ID.
   */
  std::int32_t session_id() const { return session_id_; }

  /**
   * Get the correlation ID.
   */
  std::int64_t correlation_id() const { return correlation_id_; }

  /**
   * Get the current position.
   */
  std::int64_t position() const { return position_.load(); }

  /**
   * Check if the image is closed.
   */
  bool is_closed() const { return closed_.load(); }

  /**
   * Check if the image is end of stream.
   */
  bool is_end_of_stream() const;

  /**
   * Close the image.
   */
  void close();

private:
  std::int32_t session_id_;
  std::int64_t correlation_id_;
  std::shared_ptr<util::MemoryMappedFile> log_buffer_;
  std::atomic<std::int64_t> position_{0};
  std::atomic<bool> closed_{false};

  // Term buffer management
  std::int32_t term_length_;
  std::int32_t position_bits_to_shift_;

  static constexpr std::int32_t DEFAULT_TERM_LENGTH = 64 * 1024;
};

/**
 * Subscription for receiving messages from one or more publishers.
 */
class Subscription {
public:
  /**
   * Constructor.
   */
  Subscription(const std::string &channel, std::int32_t stream_id,
               std::int64_t registration_id);

  /**
   * Destructor.
   */
  ~Subscription();

  /**
   * Poll for new messages across all images.
   *
   * @param handler Function to call for each message
   * @param fragment_limit Maximum number of fragments to process
   * @return Number of fragments processed
   */
  int poll(fragment_handler_t handler, int fragment_limit);

  /**
   * Controlled poll for new messages across all images.
   *
   * @param handler Function to call for each message
   * @param fragment_limit Maximum number of fragments to process
   * @return Number of fragments processed
   */
  int controlled_poll(controlled_fragment_handler_t handler,
                      int fragment_limit);

  /**
   * Poll for new messages from a specific image.
   *
   * @param handler Function to call for each message
   * @param fragment_limit Maximum number of fragments to process
   * @param image_session_id Session ID of specific image
   * @return Number of fragments processed
   */
  int poll_image(fragment_handler_t handler, int fragment_limit,
                 std::int32_t image_session_id);

  /**
   * Get the channel URI.
   */
  const std::string &channel() const { return channel_; }

  /**
   * Get the stream ID.
   */
  std::int32_t stream_id() const { return stream_id_; }

  /**
   * Get the registration ID.
   */
  std::int64_t registration_id() const { return registration_id_; }

  /**
   * Check if the subscription has any images.
   */
  bool has_images() const { return !images_.empty(); }

  /**
   * Get the number of images.
   */
  std::size_t image_count() const { return images_.size(); }

  /**
   * Get all images.
   */
  std::vector<std::shared_ptr<Image>> images() const;

  /**
   * Get image by session ID.
   */
  std::shared_ptr<Image> image_by_session_id(std::int32_t session_id) const;

  /**
   * Check if the subscription is connected.
   */
  bool is_connected() const;

  /**
   * Check if the subscription is closed.
   */
  bool is_closed() const { return closed_.load(); }

  /**
   * Close the subscription.
   */
  void close();

  /**
   * Add an image (called by Aeron client).
   */
  void add_image(std::shared_ptr<Image> image);

  /**
   * Remove an image (called by Aeron client).
   */
  void remove_image(std::int64_t correlation_id);

private:
  std::string channel_;
  std::int32_t stream_id_;
  std::int64_t registration_id_;
  std::atomic<bool> closed_{false};

  // Images from different publishers
  mutable std::mutex images_mutex_;
  std::vector<std::shared_ptr<Image>> images_;

  // Round-robin polling state
  std::size_t last_image_index_{0};
};

} // namespace client
} // namespace aeron
