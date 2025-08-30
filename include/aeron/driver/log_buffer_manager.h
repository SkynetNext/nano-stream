#pragma once

#include "aeron/logbuffer/frame_descriptor.h"
#include "aeron/logbuffer/log_buffers.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace aeron {
namespace driver {

/**
 * Manages log buffers for publications and subscriptions.
 * Handles the data flow between publishers and subscribers.
 */
class LogBufferManager {
public:
  struct PublicationInfo {
    std::int32_t session_id;
    std::int32_t stream_id;
    std::int64_t correlation_id;
    std::shared_ptr<logbuffer::LogBuffers> log_buffers;
    std::atomic<std::int64_t> tail_position;
    std::atomic<std::int64_t> head_position;
    std::atomic<std::int32_t> active_term_id;
    std::atomic<std::int32_t> active_term_count;
  };

  struct SubscriptionInfo {
    std::int32_t session_id;
    std::int32_t stream_id;
    std::int64_t correlation_id;
    std::shared_ptr<logbuffer::LogBuffers> log_buffers;
    std::atomic<std::int64_t> head_position;
    std::atomic<std::int64_t> tail_position;
    std::atomic<std::int32_t> active_term_id;
  };

  /**
   * Add a publication log buffer.
   */
  void add_publication(std::int32_t session_id, std::int32_t stream_id,
                       std::int64_t correlation_id,
                       std::shared_ptr<logbuffer::LogBuffers> log_buffers);

  /**
   * Remove a publication log buffer.
   */
  void remove_publication(std::int32_t session_id, std::int32_t stream_id);

  /**
   * Add a subscription log buffer.
   */
  void add_subscription(std::int32_t session_id, std::int32_t stream_id,
                        std::int64_t correlation_id,
                        std::shared_ptr<logbuffer::LogBuffers> log_buffers);

  /**
   * Remove a subscription log buffer.
   */
  void remove_subscription(std::int32_t session_id, std::int32_t stream_id);

  /**
   * Get publication info by session and stream.
   */
  std::shared_ptr<PublicationInfo> get_publication(std::int32_t session_id,
                                                   std::int32_t stream_id);

  /**
   * Get subscription info by session and stream.
   */
  std::shared_ptr<SubscriptionInfo> get_subscription(std::int32_t session_id,
                                                     std::int32_t stream_id);

  /**
   * Read data from a publication's log buffer.
   * Returns the number of bytes read.
   */
  std::int32_t read_publication_data(std::int32_t session_id,
                                     std::int32_t stream_id,
                                     std::uint8_t *buffer,
                                     std::int32_t max_length,
                                     std::int64_t &position);

  /**
   * Write data to a subscription's log buffer.
   * Returns the number of bytes written.
   */
  std::int32_t write_subscription_data(std::int32_t session_id,
                                       std::int32_t stream_id,
                                       const std::uint8_t *buffer,
                                       std::int32_t length,
                                       std::int64_t position);

  /**
   * Get all publications.
   */
  std::vector<std::shared_ptr<PublicationInfo>> get_all_publications();

  /**
   * Get all subscriptions.
   */
  std::vector<std::shared_ptr<SubscriptionInfo>> get_all_subscriptions();

private:
  std::int64_t make_key(std::int32_t session_id, std::int32_t stream_id) const {
    return (static_cast<std::int64_t>(session_id) << 32) |
           static_cast<std::uint32_t>(stream_id);
  }

  std::mutex publications_mutex_;
  std::mutex subscriptions_mutex_;

  std::unordered_map<std::int64_t, std::shared_ptr<PublicationInfo>>
      publications_;
  std::unordered_map<std::int64_t, std::shared_ptr<SubscriptionInfo>>
      subscriptions_;
};

} // namespace driver
} // namespace aeron
