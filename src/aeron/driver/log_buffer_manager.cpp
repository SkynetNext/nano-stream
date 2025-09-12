#include "aeron/driver/log_buffer_manager.h"
#include <cstring>

namespace aeron {
namespace driver {

void LogBufferManager::add_publication(
    std::int32_t session_id, std::int32_t stream_id,
    std::int64_t correlation_id,
    std::shared_ptr<logbuffer::LogBuffers> log_buffers) {
  std::lock_guard<std::mutex> lock(publications_mutex_);

  auto key = make_key(session_id, stream_id);
  auto info = std::make_shared<PublicationInfo>();

  info->session_id = session_id;
  info->stream_id = stream_id;
  info->correlation_id = correlation_id;
  info->log_buffers = log_buffers;
  info->tail_position.store(0);
  info->head_position.store(0);
  info->active_term_id.store(0);
  info->active_term_count.store(0);

  publications_[key] = info;
}

void LogBufferManager::remove_publication(std::int32_t session_id,
                                          std::int32_t stream_id) {
  std::lock_guard<std::mutex> lock(publications_mutex_);

  auto key = make_key(session_id, stream_id);
  publications_.erase(key);
}

void LogBufferManager::add_subscription(
    std::int32_t session_id, std::int32_t stream_id,
    std::int64_t correlation_id,
    std::shared_ptr<logbuffer::LogBuffers> log_buffers) {
  std::lock_guard<std::mutex> lock(subscriptions_mutex_);

  auto key = make_key(session_id, stream_id);
  auto info = std::make_shared<SubscriptionInfo>();

  info->session_id = session_id;
  info->stream_id = stream_id;
  info->correlation_id = correlation_id;
  info->log_buffers = log_buffers;
  info->head_position.store(0);
  info->tail_position.store(0);
  info->active_term_id.store(0);

  subscriptions_[key] = info;
}

void LogBufferManager::remove_subscription(std::int32_t session_id,
                                           std::int32_t stream_id) {
  std::lock_guard<std::mutex> lock(subscriptions_mutex_);

  auto key = make_key(session_id, stream_id);
  subscriptions_.erase(key);
}

std::shared_ptr<LogBufferManager::PublicationInfo>
LogBufferManager::get_publication(std::int32_t session_id,
                                  std::int32_t stream_id) {
  std::lock_guard<std::mutex> lock(publications_mutex_);

  auto key = make_key(session_id, stream_id);
  auto it = publications_.find(key);
  if (it != publications_.end()) {
    return it->second;
  }
  return nullptr;
}

std::shared_ptr<LogBufferManager::SubscriptionInfo>
LogBufferManager::get_subscription(std::int32_t session_id,
                                   std::int32_t stream_id) {
  std::lock_guard<std::mutex> lock(subscriptions_mutex_);

  auto key = make_key(session_id, stream_id);
  auto it = subscriptions_.find(key);
  if (it != subscriptions_.end()) {
    return it->second;
  }
  return nullptr;
}

std::int32_t LogBufferManager::read_publication_data(std::int32_t session_id,
                                                     std::int32_t stream_id,
                                                     std::uint8_t *buffer,
                                                     std::int32_t max_length,
                                                     std::int64_t &position) {
  auto publication = get_publication(session_id, stream_id);
  if (!publication || !publication->log_buffers) {
    return 0;
  }

  std::int64_t current_position = publication->head_position.load();

  // Read tail position directly from log buffer metadata (like Java
  // NetworkPublication)
  std::uint8_t *metadata = publication->log_buffers->logMetaDataBuffer();
  const std::int32_t tailCounterOffset =
      logbuffer::LogBufferDescriptor::TERM_TAIL_COUNTERS_OFFSET;
  std::atomic<std::int64_t> *tailCounter =
      reinterpret_cast<std::atomic<std::int64_t> *>(metadata +
                                                    tailCounterOffset);
  std::int64_t tail_position = tailCounter->load();

  if (current_position >= tail_position) {
    return 0; // No new data
  }

  std::int32_t term_length = publication->log_buffers->termLength();
  std::int32_t term_offset =
      static_cast<std::int32_t>(current_position & (term_length - 1));
  std::int32_t term_id =
      static_cast<std::int32_t>(current_position / term_length);
  std::int32_t partition_index =
      term_id % logbuffer::LogBufferDescriptor::PARTITION_COUNT;

  std::uint8_t *term_buffer = publication->log_buffers->buffer(partition_index);

  // Check if we have enough data for a frame header
  if (term_offset + logbuffer::FrameDescriptor::HEADER_LENGTH > term_length) {
    return 0;
  }

  // Read frame header
  std::int32_t frame_length =
      logbuffer::FrameDescriptor::frameLength(term_buffer, term_offset);
  if (frame_length <= 0 || frame_length > term_length - term_offset) {
    return 0;
  }

  // Check if we have enough space in the output buffer
  if (frame_length > max_length) {
    return 0;
  }

  // Copy the entire frame (header + data)
  std::memcpy(buffer, term_buffer + term_offset, frame_length);

  // Update position
  position = current_position;
  publication->head_position.store(current_position + frame_length);

  return frame_length;
}

std::int32_t LogBufferManager::write_subscription_data(
    std::int32_t session_id, std::int32_t stream_id, const std::uint8_t *buffer,
    std::int32_t length, std::int64_t position) {
  auto subscription = get_subscription(session_id, stream_id);
  if (!subscription || !subscription->log_buffers) {
    return 0;
  }

  std::int32_t term_length = subscription->log_buffers->termLength();
  std::int32_t term_offset =
      static_cast<std::int32_t>(position & (term_length - 1));
  std::int32_t term_id = static_cast<std::int32_t>(position / term_length);
  std::int32_t partition_index =
      term_id % logbuffer::LogBufferDescriptor::PARTITION_COUNT;

  // Check if we have enough space in the term
  if (term_offset + length > term_length) {
    return 0;
  }

  std::uint8_t *term_buffer =
      subscription->log_buffers->buffer(partition_index);

  // Write the data
  std::memcpy(term_buffer + term_offset, buffer, length);

  // Update tail position
  subscription->tail_position.store(position + length);

  return length;
}

std::vector<std::shared_ptr<LogBufferManager::PublicationInfo>>
LogBufferManager::get_all_publications() {
  std::lock_guard<std::mutex> lock(publications_mutex_);

  std::vector<std::shared_ptr<PublicationInfo>> result;
  result.reserve(publications_.size());

  for (const auto &pair : publications_) {
    result.push_back(pair.second);
  }

  return result;
}

std::vector<std::shared_ptr<LogBufferManager::SubscriptionInfo>>
LogBufferManager::get_all_subscriptions() {
  std::lock_guard<std::mutex> lock(subscriptions_mutex_);

  std::vector<std::shared_ptr<SubscriptionInfo>> result;
  result.reserve(subscriptions_.size());

  for (const auto &pair : subscriptions_) {
    result.push_back(pair.second);
  }

  return result;
}

} // namespace driver
} // namespace aeron
