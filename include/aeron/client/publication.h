#pragma once

#include "../../disruptor/ring_buffer.h"
#include "../util/memory_mapped_file.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace aeron {
namespace client {

/**
 * Publication result codes.
 */
enum class PublicationResult : std::int64_t {
  SUCCESS = 0,
  BACK_PRESSURED = -1,
  ADMIN_ACTION = -2,
  CLOSED = -3,
  MAX_POSITION_EXCEEDED = -4,
  NOT_CONNECTED = -5
};

/**
 * Base class for publications.
 */
class PublicationBase {
public:
  /**
   * Constructor.
   */
  PublicationBase(const std::string &channel, std::int32_t stream_id,
                  std::int32_t session_id, std::int64_t registration_id);

  /**
   * Virtual destructor.
   */
  virtual ~PublicationBase() = default;

  /**
   * Get the channel URI.
   */
  const std::string &channel() const { return channel_; }

  /**
   * Get the stream ID.
   */
  std::int32_t stream_id() const { return stream_id_; }

  /**
   * Get the session ID.
   */
  std::int32_t session_id() const { return session_id_; }

  /**
   * Get the registration ID.
   */
  std::int64_t registration_id() const { return registration_id_; }

  /**
   * Check if the publication is connected.
   */
  virtual bool is_connected() const = 0;

  /**
   * Check if the publication is closed.
   */
  bool is_closed() const { return closed_.load(); }

  /**
   * Get the current position.
   */
  virtual std::int64_t position() const = 0;

  /**
   * Get the position limit (flow control).
   */
  virtual std::int64_t position_limit() const = 0;

  /**
   * Close the publication.
   */
  virtual void close();

protected:
  std::string channel_;
  std::int32_t stream_id_;
  std::int32_t session_id_;
  std::int64_t registration_id_;
  std::atomic<bool> closed_{false};
};

/**
 * Publication for sending messages (multiple publishers allowed).
 */
class Publication : public PublicationBase {
public:
  /**
   * Constructor.
   */
  Publication(const std::string &channel, std::int32_t stream_id,
              std::int32_t session_id, std::int64_t registration_id,
              std::shared_ptr<util::MemoryMappedFile> log_buffer);

  /**
   * Offer a message for publication.
   *
   * @param buffer Message data
   * @param offset Offset in buffer
   * @param length Length of message
   * @return Publication result
   */
  PublicationResult offer(const std::uint8_t *buffer, std::size_t offset,
                          std::size_t length);

  /**
   * Offer a message for publication.
   *
   * @param buffer Message data
   * @param length Length of message
   * @return Publication result
   */
  PublicationResult offer(const std::uint8_t *buffer, std::size_t length) {
    return offer(buffer, 0, length);
  }

  /**
   * Try to claim space for a message.
   *
   * @param length Length of message to claim
   * @return Claimed buffer position, or negative error code
   */
  std::int64_t try_claim(std::size_t length);

  /**
   * Commit a previously claimed message.
   *
   * @param position Position returned by try_claim
   */
  void commit(std::int64_t position);

  /**
   * Abort a previously claimed message.
   *
   * @param position Position returned by try_claim
   */
  void abort(std::int64_t position);

  /**
   * Check if the publication is connected.
   */
  bool is_connected() const override;

  /**
   * Get the current position.
   */
  std::int64_t position() const override;

  /**
   * Get the position limit (flow control).
   */
  std::int64_t position_limit() const override;

  /**
   * Get the maximum possible position.
   */
  std::int64_t max_possible_position() const;

private:
  std::shared_ptr<util::MemoryMappedFile> log_buffer_;
  std::atomic<std::int64_t> position_{0};
  std::atomic<std::int64_t> position_limit_{0};

  // Term buffer management
  std::int32_t term_length_;
  std::int32_t max_message_length_;
  std::int32_t max_payload_length_;

  static constexpr std::int32_t DEFAULT_TERM_LENGTH = 64 * 1024;
  static constexpr std::int32_t MAX_MESSAGE_LENGTH = 16 * 1024 * 1024;
};

/**
 * Exclusive publication for sending messages (single publisher only).
 * Provides better performance than regular Publication when only one
 * thread is publishing.
 */
class ExclusivePublication : public PublicationBase {
public:
  /**
   * Constructor.
   */
  ExclusivePublication(const std::string &channel, std::int32_t stream_id,
                       std::int32_t session_id, std::int64_t registration_id,
                       std::shared_ptr<util::MemoryMappedFile> log_buffer);

  /**
   * Offer a message for publication.
   *
   * @param buffer Message data
   * @param offset Offset in buffer
   * @param length Length of message
   * @return Publication result
   */
  PublicationResult offer(const std::uint8_t *buffer, std::size_t offset,
                          std::size_t length);

  /**
   * Offer a message for publication.
   *
   * @param buffer Message data
   * @param length Length of message
   * @return Publication result
   */
  PublicationResult offer(const std::uint8_t *buffer, std::size_t length) {
    return offer(buffer, 0, length);
  }

  /**
   * Try to claim space for a message.
   *
   * @param length Length of message to claim
   * @return Claimed buffer position, or negative error code
   */
  std::int64_t try_claim(std::size_t length);

  /**
   * Commit a previously claimed message.
   *
   * @param position Position returned by try_claim
   */
  void commit(std::int64_t position);

  /**
   * Abort a previously claimed message.
   *
   * @param position Position returned by try_claim
   */
  void abort(std::int64_t position);

  /**
   * Check if the publication is connected.
   */
  bool is_connected() const override;

  /**
   * Get the current position.
   */
  std::int64_t position() const override;

  /**
   * Get the position limit (flow control).
   */
  std::int64_t position_limit() const override;

  /**
   * Get the maximum possible position.
   */
  std::int64_t max_possible_position() const;

private:
  std::shared_ptr<util::MemoryMappedFile> log_buffer_;
  std::int64_t position_{0}; // No atomic needed for exclusive access
  std::atomic<std::int64_t> position_limit_{0};

  // Term buffer management
  std::int32_t term_length_;
  std::int32_t max_message_length_;
  std::int32_t max_payload_length_;

  static constexpr std::int32_t DEFAULT_TERM_LENGTH = 64 * 1024;
  static constexpr std::int32_t MAX_MESSAGE_LENGTH = 16 * 1024 * 1024;
};

} // namespace client
} // namespace aeron
