#pragma once

#include <cstdint>

namespace aeron {
namespace command {

/**
 * Control Protocol Events for communication between client and Media Driver.
 * Based on Aeron's ControlProtocolEvents.
 */
namespace ControlProtocolEvents {

// ========== Command Types (Client to Driver) ==========

/**
 * Add publication command.
 */
static constexpr std::int32_t ADD_PUBLICATION = 0x01;

/**
 * Remove publication command.
 */
static constexpr std::int32_t REMOVE_PUBLICATION = 0x02;

/**
 * Add exclusive publication command.
 */
static constexpr std::int32_t ADD_EXCLUSIVE_PUBLICATION = 0x03;

/**
 * Add subscription command.
 */
static constexpr std::int32_t ADD_SUBSCRIPTION = 0x04;

/**
 * Remove subscription command.
 */
static constexpr std::int32_t REMOVE_SUBSCRIPTION = 0x05;

/**
 * Client keepalive command.
 */
static constexpr std::int32_t CLIENT_KEEPALIVE = 0x06;

/**
 * Add destination command.
 */
static constexpr std::int32_t ADD_DESTINATION = 0x07;

/**
 * Remove destination command.
 */
static constexpr std::int32_t REMOVE_DESTINATION = 0x08;

/**
 * Terminate driver command.
 */
static constexpr std::int32_t TERMINATE_DRIVER = 0x09;

// ========== Response Types (Driver to Client) ==========

/**
 * Operation succeeded response.
 */
static constexpr std::int32_t ON_OPERATION_SUCCESS = 0x0A;

/**
 * Error response.
 */
static constexpr std::int32_t ON_ERROR = 0x0B;

/**
 * Publication ready response.
 */
static constexpr std::int32_t ON_PUBLICATION_READY = 0x0C;

/**
 * Subscription ready response.
 */
static constexpr std::int32_t ON_SUBSCRIPTION_READY = 0x0D;

/**
 * Available image response.
 */
static constexpr std::int32_t ON_AVAILABLE_IMAGE = 0x0E;

/**
 * Unavailable image response.
 */
static constexpr std::int32_t ON_UNAVAILABLE_IMAGE = 0x0F;

/**
 * Counter ready response.
 */
static constexpr std::int32_t ON_COUNTER_READY = 0x10;

/**
 * Unavailable counter response.
 */
static constexpr std::int32_t ON_UNAVAILABLE_COUNTER = 0x11;

/**
 * Client timeout response.
 */
static constexpr std::int32_t ON_CLIENT_TIMEOUT = 0x12;

} // namespace ControlProtocolEvents

/**
 * Error codes for error responses.
 */
namespace ErrorCode {

/**
 * Invalid channel.
 */
static constexpr std::int32_t INVALID_CHANNEL = 1;

/**
 * Unknown subscription.
 */
static constexpr std::int32_t UNKNOWN_SUBSCRIPTION = 2;

/**
 * Unknown publication.
 */
static constexpr std::int32_t UNKNOWN_PUBLICATION = 3;

/**
 * Channel endpoint error.
 */
static constexpr std::int32_t CHANNEL_ENDPOINT_ERROR = 4;

/**
 * Unknown counter.
 */
static constexpr std::int32_t UNKNOWN_COUNTER = 5;

/**
 * Unknown command type.
 */
static constexpr std::int32_t UNKNOWN_COMMAND_TYPE = 6;

/**
 * Malformed command.
 */
static constexpr std::int32_t MALFORMED_COMMAND = 7;

/**
 * Not supported.
 */
static constexpr std::int32_t NOT_SUPPORTED = 8;

/**
 * Storage space error.
 */
static constexpr std::int32_t STORAGE_SPACE = 9;

/**
 * Generic error.
 */
static constexpr std::int32_t GENERIC_ERROR = 10;

} // namespace ErrorCode

} // namespace command
} // namespace aeron
