#pragma once

#include "aeron/driver/media/send_channel_endpoint.h"
#include "aeron/logbuffer/log_buffers.h"
#include <atomic>
#include <cstdint>
#include <memory>

namespace aeron {
namespace driver {

/**
 * NetworkPublication represents a publication that can send data over the
 * network. This is the C++ equivalent of Java's NetworkPublication.
 */
class NetworkPublication {
public:
  NetworkPublication(
      std::shared_ptr<logbuffer::LogBuffers> logBuffers,
      std::shared_ptr<media::SendChannelEndpoint> channelEndpoint,
      std::int32_t sessionId, std::int32_t streamId, std::int32_t initialTermId,
      std::int32_t termLength, std::int32_t mtuLength);

  /**
   * Send data from the log buffer to the network.
   * This is the main method called by Sender.
   */
  std::int32_t send(std::int64_t nowNs);

  /**
   * Get the session ID.
   */
  std::int32_t sessionId() const { return sessionId_; }

  /**
   * Get the stream ID.
   */
  std::int32_t streamId() const { return streamId_; }

  /**
   * Get the channel endpoint.
   */
  std::shared_ptr<media::SendChannelEndpoint> channelEndpoint() const {
    return channelEndpoint_;
  }

  /**
   * Release resources when Sender is done with this publication.
   */
  void senderRelease();

  /**
   * Get the producer position from log buffer metadata.
   */
  std::int64_t producerPosition() const;

  /**
   * Get the sender position.
   */
  std::int64_t senderPosition() const { return senderPosition_.load(); }

private:
  std::shared_ptr<logbuffer::LogBuffers> logBuffers_;
  std::shared_ptr<media::SendChannelEndpoint> channelEndpoint_;
  std::int32_t sessionId_;
  std::int32_t streamId_;
  std::int32_t initialTermId_;
  std::int32_t termLength_;
  std::int32_t mtuLength_;
  std::int32_t positionBitsToShift_;
  std::int32_t termLengthMask_;

  std::atomic<std::int64_t> senderPosition_{0};
  std::atomic<std::int64_t> cleanPosition_{0};
  std::int64_t timeOfLastActivityNs_{0};
  std::int64_t lastSenderPosition_{0};

  std::atomic<std::int32_t> refCount_{0};
  bool hasSenderReleased_{false};

  /**
   * Send data frames from the current term buffer.
   */
  std::int32_t sendData(std::int64_t nowNs, std::int64_t senderPosition,
                        std::int32_t termOffset);

  /**
   * Send setup frames for new connections.
   */
  void setupMessageCheck(std::int64_t nowNs, std::int32_t activeTermId,
                         std::int32_t termOffset);

  /**
   * Send heartbeat messages.
   */
  std::int32_t heartbeatMessageCheck(std::int64_t nowNs,
                                     std::int32_t activeTermId,
                                     std::int32_t termOffset);
};

} // namespace driver
} // namespace aeron
