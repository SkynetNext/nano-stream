#pragma once

/**
 * Nano-Stream Aeron - Complete Header-Only Aeron Implementation
 *
 * This is the main include file for the Nano-Stream Aeron library.
 * It provides a complete, header-only implementation of the Aeron messaging
 * system designed for ultra-low latency, high-throughput inter-process
 * communication.
 *
 * Features:
 * - Complete Aeron API compatibility with real Media Driver
 * - Full UDP and IPC transport implementations
 * - Header-only design for easy integration
 * - Zero-copy message passing
 * - Lock-free data structures
 * - Memory-mapped IPC transport
 * - Publisher/Subscriber pattern
 * - Fragmented message support
 * - Back pressure handling
 * - NAK-based retransmission
 * - Flow control mechanisms
 *
 * Usage:
 *   #include "nano_stream_aeron.h"
 *
 *   // Start Media Driver
 *   auto driver = aeron::driver::MediaDriver::launch();
 *
 *   // Connect client
 *   auto aeron = aeron::Aeron::connect();
 *   auto publication =
 * aeron->add_publication("aeron:udp?endpoint=localhost:40124", 1001); auto
 * subscription = aeron->add_subscription("aeron:udp?endpoint=localhost:40124",
 * 1001);
 *
 * @author Nano-Stream Project
 * @version 2.0.0 - Complete Aeron Implementation
 */

// ========== Core Aeron Client API ==========
#include "aeron/aeron.h"
#include "aeron/client_conductor.h"
#include "aeron/context.h"
#include "aeron/log_buffers.h" // IWYU pragma: export
#include "aeron/publication.h"
#include "aeron/subscription.h"

// ========== Media Driver ==========
#include "aeron/driver/driver_conductor.h" // IWYU pragma: export
#include "aeron/driver/media_driver.h"
#include "aeron/driver/receiver.h" // IWYU pragma: export
#include "aeron/driver/sender.h"   // IWYU pragma: export

// ========== Concurrent Utilities ==========
#include "aeron/concurrent/atomic_buffer.h" // IWYU pragma: export
#include "aeron/concurrent/logbuffer/log_buffer_descriptor.h" // IWYU pragma: export

// ========== System Utilities ==========
#include "aeron/util/bit_util.h"           // IWYU pragma: export
#include "aeron/util/memory_mapped_file.h" // IWYU pragma: export

// ========== Command Protocol ==========
#include "aeron/command/control_protocol_events.h" // IWYU pragma: export

// ========== IPC Transport ==========
#include "aeron/ipc/ipc_publication.h"  // IWYU pragma: export
#include "aeron/ipc/ipc_subscription.h" // IWYU pragma: export
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

/**
 * Nano-Stream Aeron namespace with convenience utilities.
 */
namespace nano_stream_aeron {

/**
 * Quick start configurations for common use cases.
 */
namespace presets {

/**
 * Ultra-low latency configuration.
 */
inline std::unique_ptr<aeron::driver::DriverContext>
ultra_low_latency_driver() {
  auto context = std::make_unique<aeron::driver::DriverContext>();
  context->threading_mode(aeron::driver::ThreadingMode::DEDICATED)
      .publication_term_buffer_length(64 * 1024)
      .ipc_publication_term_buffer_length(64 * 1024);
  return context;
}

/**
 * Ultra-low latency client configuration.
 */
inline std::unique_ptr<aeron::Context> ultra_low_latency_client() {
  auto context = std::make_unique<aeron::Context>();
  context->publication_term_buffer_length(64 * 1024)
      .ipc_term_buffer_length(64 * 1024)
      .pre_touch_mapped_memory(true);
  return context;
}

/**
 * High throughput driver configuration.
 */
inline std::unique_ptr<aeron::driver::DriverContext> high_throughput_driver() {
  auto context = std::make_unique<aeron::driver::DriverContext>();
  context->threading_mode(aeron::driver::ThreadingMode::SHARED_NETWORK)
      .publication_term_buffer_length(16 * 1024 * 1024)
      .ipc_publication_term_buffer_length(16 * 1024 * 1024);
  return context;
}

/**
 * High throughput client configuration.
 */
inline std::unique_ptr<aeron::Context> high_throughput_client() {
  auto context = std::make_unique<aeron::Context>();
  context->publication_term_buffer_length(16 * 1024 * 1024)
      .ipc_term_buffer_length(16 * 1024 * 1024)
      .pre_touch_mapped_memory(true);
  return context;
}

/**
 * Testing/development driver configuration.
 */
inline std::unique_ptr<aeron::driver::DriverContext> testing_driver() {
  auto context = std::make_unique<aeron::driver::DriverContext>();
  context->aeron_dir("/tmp/aeron-test")
      .threading_mode(aeron::driver::ThreadingMode::SHARED)
      .publication_term_buffer_length(256 * 1024)
      .ipc_publication_term_buffer_length(256 * 1024);
  return context;
}

/**
 * Testing/development client configuration.
 */
inline std::unique_ptr<aeron::Context> testing_client() {
  auto context = std::make_unique<aeron::Context>();
  context->aeron_dir("/tmp/aeron-test")
      .publication_term_buffer_length(256 * 1024)
      .ipc_term_buffer_length(256 * 1024);
  return context;
}

} // namespace presets

/**
 * High-level API for common patterns.
 */
namespace patterns {

/**
 * Simple request-response pattern.
 */
class RequestResponse {
public:
  /**
   * Constructor for RequestResponse.
   */
  RequestResponse(std::shared_ptr<aeron::Aeron> aeron,
                  const std::string &request_channel,
                  std::int32_t request_stream,
                  const std::string &response_channel,
                  std::int32_t response_stream);

  /**
   * Send a request and wait for response.
   */
  std::string send_request(const std::string &request,
                           std::int64_t timeout_ms = 5000);

  /**
   * Start processing requests with a handler.
   */
  void start_request_handler(
      std::function<std::string(const std::string &)> handler);

  /**
   * Stop processing requests.
   */
  void stop();

private:
  std::shared_ptr<aeron::Aeron> aeron_;
  std::shared_ptr<aeron::Publication> request_publication_;
  std::shared_ptr<aeron::Subscription> request_subscription_;
  std::shared_ptr<aeron::Publication> response_publication_;
  std::shared_ptr<aeron::Subscription> response_subscription_;

  std::atomic<bool> running_;
  std::thread handler_thread_;
  std::function<std::string(const std::string &)> request_handler_;
};

/**
 * Publish-subscribe pattern with multiple subscribers.
 */
class PublishSubscribe {
public:
  /**
   * Publisher constructor.
   */
  PublishSubscribe(std::shared_ptr<aeron::Aeron> aeron,
                   const std::string &channel, std::int32_t stream);

  /**
   * Publish a message to all subscribers.
   */
  bool publish(const std::string &message);

  /**
   * Subscribe with a message handler.
   */
  void subscribe(std::function<void(const std::string &)> handler);

  /**
   * Unsubscribe.
   */
  void unsubscribe();

private:
  std::shared_ptr<aeron::Aeron> aeron_;
  std::shared_ptr<aeron::Publication> publication_;
  std::shared_ptr<aeron::Subscription> subscription_;
  std::function<void(const std::string &)> message_handler_;

  std::atomic<bool> running_;
  std::thread subscriber_thread_;
};

} // namespace patterns

/**
 * Performance monitoring utilities.
 */
namespace monitoring {

/**
 * Performance metrics collector.
 */
class PerformanceMonitor {
public:
  /**
   * Constructor.
   */
  PerformanceMonitor();

  /**
   * Record a latency measurement.
   */
  void record_latency(std::int64_t latency_ns);

  /**
   * Record throughput measurement.
   */
  void record_throughput(std::int64_t messages_per_second);

  /**
   * Get average latency.
   */
  double average_latency_ns() const;

  /**
   * Get 99th percentile latency.
   */
  std::int64_t p99_latency_ns() const;

  /**
   * Get current throughput.
   */
  std::int64_t current_throughput() const;

  /**
   * Reset all metrics.
   */
  void reset();

private:
  std::vector<std::int64_t> latency_samples_;
  std::atomic<std::int64_t> total_latency_;
  std::atomic<std::int64_t> sample_count_;
  std::atomic<std::int64_t> current_throughput_;
};

} // namespace monitoring

} // namespace nano_stream_aeron

/**
 * Convenience aliases for commonly used types.
 */
// namespace nsa = nano_stream_aeron;  // Uncomment if needed

/**
 * Quick start macros for common operations.
 */
#define AERON_QUICK_START_DRIVER()                                             \
  auto __driver = aeron::driver::MediaDriver::launch(                          \
      nano_stream_aeron::presets::testing_driver())

#define AERON_QUICK_START_CLIENT()                                             \
  auto __aeron =                                                               \
      aeron::Aeron::connect(nano_stream_aeron::presets::testing_client())

#define AERON_IPC_PUB(stream_id)                                               \
  __aeron->add_publication("aeron:ipc", stream_id)

#define AERON_IPC_SUB(stream_id)                                               \
  __aeron->add_subscription("aeron:ipc", stream_id)

#define AERON_UDP_PUB(endpoint, stream_id)                                     \
  __aeron->add_publication("aeron:udp?endpoint=" endpoint, stream_id)

#define AERON_UDP_SUB(endpoint, stream_id)                                     \
  __aeron->add_subscription("aeron:udp?endpoint=" endpoint, stream_id)