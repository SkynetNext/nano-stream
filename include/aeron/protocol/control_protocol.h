#pragma once

#include <cstdint>
#include <cstring>

namespace aeron {
namespace protocol {

/**
 * Control message types for client-driver communication.
 */
enum class ControlMessageType : std::int32_t {
  ADD_PUBLICATION = 0x01,
  REMOVE_PUBLICATION = 0x02,
  ADD_SUBSCRIPTION = 0x11,
  REMOVE_SUBSCRIPTION = 0x12,
  CLIENT_KEEPALIVE = 0x20,
  ADD_DESTINATION = 0x30,
  REMOVE_DESTINATION = 0x31,
  ADD_RCV_DESTINATION = 0x32,
  REMOVE_RCV_DESTINATION = 0x33,
  CLIENT_CLOSE = 0x40,
  ADD_COUNTER = 0x50,
  REMOVE_COUNTER = 0x51,
  CLIENT_TIMEOUT = 0x60
};

/**
 * Base control message header.
 */
struct ControlMessageHeader {
  std::int32_t length;         // Total message length
  ControlMessageType type;     // Message type
  std::int64_t correlation_id; // Correlation ID for response matching
  std::int64_t client_id;      // Client identifier

  void init(ControlMessageType msg_type, std::int64_t corr_id,
            std::int64_t cli_id) {
    type = msg_type;
    correlation_id = corr_id;
    client_id = cli_id;
  }
};

/**
 * Publication control message.
 */
struct PublicationMessage {
  ControlMessageHeader header;
  std::int32_t stream_id;
  std::int32_t session_id;
  std::int32_t channel_length;
  // Variable length channel string follows

  static constexpr std::int32_t BASE_LENGTH =
      sizeof(ControlMessageHeader) + 3 * sizeof(std::int32_t);

  void init(std::int64_t correlation_id, std::int64_t client_id,
            std::int32_t stream, std::int32_t session, const char *channel) {
    header.init(ControlMessageType::ADD_PUBLICATION, correlation_id, client_id);
    stream_id = stream;
    session_id = session;
    channel_length = static_cast<std::int32_t>(std::strlen(channel));
    header.length = BASE_LENGTH + channel_length;
  }

  char *channel_data() { return reinterpret_cast<char *>(this) + BASE_LENGTH; }

  const char *channel_data() const {
    return reinterpret_cast<const char *>(this) + BASE_LENGTH;
  }
};

/**
 * Subscription control message.
 */
struct SubscriptionMessage {
  ControlMessageHeader header;
  std::int32_t stream_id;
  std::int64_t registration_id;
  std::int32_t channel_length;
  // Variable length channel string follows

  static constexpr std::int32_t BASE_LENGTH =
      sizeof(ControlMessageHeader) + sizeof(std::int32_t) +
      sizeof(std::int64_t) + sizeof(std::int32_t);

  void init(std::int64_t correlation_id, std::int64_t client_id,
            std::int32_t stream, std::int64_t reg_id, const char *channel) {
    header.init(ControlMessageType::ADD_SUBSCRIPTION, correlation_id,
                client_id);
    stream_id = stream;
    registration_id = reg_id;
    channel_length = static_cast<std::int32_t>(std::strlen(channel));
    header.length = BASE_LENGTH + channel_length;
  }

  char *channel_data() { return reinterpret_cast<char *>(this) + BASE_LENGTH; }

  const char *channel_data() const {
    return reinterpret_cast<const char *>(this) + BASE_LENGTH;
  }
};

/**
 * Client keepalive message.
 */
struct ClientKeepaliveMessage {
  ControlMessageHeader header;

  void init(std::int64_t correlation_id, std::int64_t client_id) {
    header.init(ControlMessageType::CLIENT_KEEPALIVE, correlation_id,
                client_id);
    header.length = sizeof(*this);
  }
};

/**
 * Response message from driver to client.
 */
enum class ResponseCode : std::int32_t {
  ON_PUBLICATION_READY = 0x01,
  ON_SUBSCRIPTION_READY = 0x02,
  ON_OPERATION_SUCCESS = 0x03,
  ON_ERROR = 0x04,
  ON_UNAVAILABLE_COUNTER = 0x05,
  ON_AVAILABLE_COUNTER = 0x06,
  ON_COUNTER_READY = 0x07,
  ON_UNAVAILABLE_IMAGE = 0x08,
  ON_AVAILABLE_IMAGE = 0x09,
  ON_CLIENT_TIMEOUT = 0x0A
};

/**
 * Driver response message.
 */
struct ResponseMessage {
  std::int32_t length;
  ResponseCode type;
  std::int64_t correlation_id;
  std::int64_t registration_id;
  std::int32_t error_code;
  std::int32_t error_message_length;
  // Variable length error message follows

  static constexpr std::int32_t BASE_LENGTH =
      sizeof(std::int32_t) + sizeof(ResponseCode) + 2 * sizeof(std::int64_t) +
      2 * sizeof(std::int32_t);

  void init_success(std::int64_t correlation_id, std::int64_t registration_id) {
    type = ResponseCode::ON_OPERATION_SUCCESS;
    this->correlation_id = correlation_id;
    this->registration_id = registration_id;
    error_code = 0;
    error_message_length = 0;
    length = BASE_LENGTH;
  }

  void init_error(std::int64_t correlation_id, std::int32_t err_code,
                  const char *message) {
    type = ResponseCode::ON_ERROR;
    this->correlation_id = correlation_id;
    registration_id = -1;
    error_code = err_code;
    error_message_length = static_cast<std::int32_t>(std::strlen(message));
    length = BASE_LENGTH + error_message_length;
  }

  char *error_message_data() {
    return reinterpret_cast<char *>(this) + BASE_LENGTH;
  }

  const char *error_message_data() const {
    return reinterpret_cast<const char *>(this) + BASE_LENGTH;
  }
};

} // namespace protocol
} // namespace aeron
