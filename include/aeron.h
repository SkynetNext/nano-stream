#pragma once

// Prevent winsock.h inclusion on Windows to avoid conflicts with winsock2.h
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_ // Prevent winsock.h from being included
#endif
#endif

/**
 * Aeron C++ Implementation
 *
 * A high-performance messaging library for low-latency, high-throughput
 * communication. Based on the LMAX Disruptor pattern with network transport.
 *
 * This implementation provides:
 * - Lock-free, wait-free data structures
 * - UDP unicast and multicast transport
 * - IPC (Inter-Process Communication) via shared memory
 * - Flow control and reliable delivery
 * - Zero-copy message passing
 *
 * Usage:
 *   #include "aeron.h"
 *
 *   // Start Media Driver
 *   auto driver = aeron::driver::MediaDriver::launch();
 *
 *   // Connect client
 *   auto aeron = aeron::client::Aeron::connect();
 *
 *   // Create publication and subscription
 *   auto pub = aeron->add_publication("aeron:udp?endpoint=localhost:40456",
 * 1001); auto sub =
 * aeron->add_subscription("aeron:udp?endpoint=localhost:40456", 1001);
 */

// Core nano-stream foundation
#include "nano_stream/nano_stream.h" // IWYU pragma: export

// Aeron driver components
#include "aeron/driver/conductor/conductor.h" // IWYU pragma: export
#include "aeron/driver/media_driver.h"
#include "aeron/driver/receiver/receiver.h"       // IWYU pragma: export
#include "aeron/driver/sender/sender.h"           // IWYU pragma: export
#include "aeron/driver/status/counters_manager.h" // IWYU pragma: export

// Aeron client API
#include "aeron/client/aeron.h"
#include "aeron/client/publication.h"
#include "aeron/client/subscription.h"

// Protocol definitions
#include "aeron/protocol/control_protocol.h" // IWYU pragma: export
#include "aeron/protocol/header.h"           // IWYU pragma: export

// Utilities
#include "aeron/util/memory_mapped_file.h" // IWYU pragma: export
#include "aeron/util/path_utils.h"         // IWYU pragma: export

namespace aeron {

/**
 * Version information for the Aeron C++ implementation.
 */
struct Version {
  static constexpr int MAJOR = 1;
  static constexpr int MINOR = 0;
  static constexpr int PATCH = 0;

  static std::string get_version_string() {
    return std::to_string(MAJOR) + "." + std::to_string(MINOR) + "." +
           std::to_string(PATCH);
  }

  static std::string get_full_version_string() {
    return "Aeron C++ " + get_version_string() + " (based on nano-stream)";
  }
};

/**
 * Quick start helper functions.
 */
namespace quick {

/**
 * Launch Media Driver with default configuration.
 */
inline std::unique_ptr<driver::MediaDriver> launch_driver() {
  return driver::MediaDriver::launch();
}

/**
 * Connect Aeron client with default configuration.
 */
inline std::shared_ptr<client::Aeron> connect() {
  return client::Aeron::connect();
}

/**
 * Create a simple publisher-subscriber pair for testing.
 */
inline std::pair<std::shared_ptr<client::Publication>,
                 std::shared_ptr<client::Subscription>>
create_pub_sub(const std::string &channel, std::int32_t stream_id) {
  auto aeron = connect();
  auto pub = aeron->add_publication(channel, stream_id);
  auto sub = aeron->add_subscription(channel, stream_id);
  return {pub, sub};
}

} // namespace quick

} // namespace aeron

// Convenience aliases for common usage
using AeronDriver = aeron::driver::MediaDriver;
using AeronClient = aeron::client::Aeron;
using AeronPublication = aeron::client::Publication;
using AeronSubscription = aeron::client::Subscription;
