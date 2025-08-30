#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#endif

#include "aeron/driver/publication/network_publication.h"
#include "aeron/logbuffer/log_buffer_descriptor.h"
#include "aeron/logbuffer/term_scanner.h"
#include "aeron/protocol/header.h"
#include <algorithm>
#include <cstring>

namespace aeron {
namespace driver {

NetworkPublication::NetworkPublication(
    std::shared_ptr<logbuffer::LogBuffers> logBuffers,
    std::shared_ptr<media::SendChannelEndpoint> channelEndpoint,
    std::int32_t sessionId, std::int32_t streamId, std::int32_t initialTermId,
    std::int32_t termLength, std::int32_t mtuLength)
    : logBuffers_(logBuffers), channelEndpoint_(channelEndpoint),
      sessionId_(sessionId), streamId_(streamId), initialTermId_(initialTermId),
      termLength_(termLength), mtuLength_(mtuLength),
      positionBitsToShift_(
          logbuffer::LogBufferDescriptor::positionBitsToShift(termLength)),
      termLengthMask_(termLength - 1) {}

std::int32_t NetworkPublication::send(std::int64_t nowNs) {
  std::int32_t bytesSent = 0;

  // Get current positions
  std::int64_t senderPosition = senderPosition_.load();
  std::int64_t producerPosition = this->producerPosition();

  if (senderPosition >= producerPosition) {
    return 0; // No data to send
  }

  // Calculate term offset and active term ID
  std::int32_t termOffset =
      static_cast<std::int32_t>(senderPosition & termLengthMask_);
  std::int32_t activeTermId =
      logbuffer::LogBufferDescriptor::computeTermIdFromPosition(
          senderPosition, positionBitsToShift_, initialTermId_);

  // Send data frames
  bytesSent += sendData(nowNs, senderPosition, termOffset);

  // Send setup frames for new connections
  setupMessageCheck(nowNs, activeTermId, termOffset);

  // Send heartbeat messages
  bytesSent += heartbeatMessageCheck(nowNs, activeTermId, termOffset);

  return bytesSent;
}

std::int32_t NetworkPublication::sendData(std::int64_t nowNs,
                                          std::int64_t senderPosition,
                                          std::int32_t termOffset) {
  std::int32_t bytesSent = 0;

  // Get available window (simplified - no flow control for now)
  std::int32_t availableWindow =
      static_cast<std::int32_t>(producerPosition() - senderPosition);

  if (availableWindow > 0) {
    std::int32_t scanLimit = std::min(availableWindow, mtuLength_);
    std::int32_t activeIndex = logbuffer::LogBufferDescriptor::indexByPosition(
        senderPosition, positionBitsToShift_);

    // Get term buffer
    std::uint8_t *termBuffer = logBuffers_->buffer(activeIndex);

    // Scan for available data
    std::int64_t scanOutcome = logbuffer::TermScanner::scanForAvailability(
        termBuffer, termOffset, scanLimit, termLength_);

    std::int32_t available = logbuffer::TermScanner::available(scanOutcome);

    if (available > 0) {
      // Send data directly from term buffer
      std::int32_t sent =
          channelEndpoint_->send(termBuffer + termOffset, available);

      if (sent == available) {
        timeOfLastActivityNs_ = nowNs;
        bytesSent = available + logbuffer::TermScanner::padding(scanOutcome);
        senderPosition_.store(senderPosition + bytesSent);
      }
    }
  }

  return bytesSent;
}

void NetworkPublication::setupMessageCheck(std::int64_t nowNs,
                                           std::int32_t activeTermId,
                                           std::int32_t termOffset) {
  // Simplified setup message - just send basic setup frame
  protocol::SetupHeader setupHeader;
  setupHeader.init();
  setupHeader.session_id = sessionId_;
  setupHeader.stream_id = streamId_;
  setupHeader.active_term_id = activeTermId;
  setupHeader.term_offset = termOffset;
  setupHeader.term_length = termLength_;
  setupHeader.mtu_length = mtuLength_;

  // Send setup frame
  channelEndpoint_->send(reinterpret_cast<const std::uint8_t *>(&setupHeader),
                         protocol::SetupHeader::HEADER_LENGTH);
}

std::int32_t NetworkPublication::heartbeatMessageCheck(
    std::int64_t nowNs, std::int32_t activeTermId, std::int32_t termOffset) {
  // Simplified heartbeat - just send basic data frame with heartbeat flags
  protocol::DataHeader heartbeatHeader;
  heartbeatHeader.init();
  heartbeatHeader.session_id = sessionId_;
  heartbeatHeader.stream_id = streamId_;
  heartbeatHeader.term_id = activeTermId;
  heartbeatHeader.term_offset = termOffset;
  heartbeatHeader.set_unfragmented();
  heartbeatHeader.set_frame_length(protocol::DataHeader::HEADER_LENGTH);

  std::int32_t sent = channelEndpoint_->send(
      reinterpret_cast<const std::uint8_t *>(&heartbeatHeader),
      protocol::DataHeader::HEADER_LENGTH);

  if (sent == protocol::DataHeader::HEADER_LENGTH) {
    timeOfLastActivityNs_ = nowNs;
  }

  return sent;
}

void NetworkPublication::senderRelease() { hasSenderReleased_ = true; }

std::int64_t NetworkPublication::producerPosition() const {
  // Read tail position from log buffer metadata
  std::uint8_t *metadata = logBuffers_->logMetaDataBuffer();
  const std::int32_t tailCounterOffset =
      logbuffer::LogBufferDescriptor::TERM_TAIL_COUNTERS_OFFSET;
  std::atomic<std::int64_t> *tailCounter =
      reinterpret_cast<std::atomic<std::int64_t> *>(metadata +
                                                    tailCounterOffset);
  return tailCounter->load();
}

} // namespace driver
} // namespace aeron
