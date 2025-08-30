#pragma once

#include "../util/path_utils.h"
#include "conductor/conductor.h"
#include "receiver/receiver.h"
#include "sender/sender.h"
#include "status/counters_manager.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace aeron {
namespace driver {

/**
 * Configuration for the Media Driver.
 */
struct MediaDriverContext {
  // Directory paths
  std::string aeron_dir = util::PathUtils::get_default_aeron_dir();
  std::string driver_timeout_ms = "10000";

  // Network configuration
  std::int32_t mtu_length = 1408;
  std::int32_t socket_rcvbuf = 128 * 1024;
  std::int32_t socket_sndbuf = 128 * 1024;

  // Threading configuration
  bool threading_mode_shared = false;
  std::int32_t conductor_idle_strategy_max_spins = 100;
  std::int32_t sender_idle_strategy_max_spins = 100;
  std::int32_t receiver_idle_strategy_max_spins = 100;

  // Memory configuration
  std::int32_t term_buffer_sparse_file = 1;
  std::int32_t term_buffer_length = 64 * 1024;
  std::int32_t ipc_term_buffer_length = 64 * 1024;
  std::int32_t publication_unblock_timeout_ns = 10000000; // 10ms

  // Flow control
  std::int32_t initial_window_length = 128 * 1024;
  std::int32_t status_message_timeout_ns = 200000000; // 200ms

  // Cleanup (using int64_t for large nanosecond values)
  std::int64_t client_liveness_timeout_ns = 5000000000LL;      // 5s
  std::int64_t publication_liveness_timeout_ns = 5000000000LL; // 5s
  std::int64_t image_liveness_timeout_ns = 10000000000LL;      // 10s
};

/**
 * The Media Driver is the core component that manages all network
 * communication and coordinates between clients, publications, and
 * subscriptions.
 *
 * It runs three main threads:
 * - Conductor: Manages client connections and lifecycle
 * - Sender: Handles outbound network traffic
 * - Receiver: Handles inbound network traffic
 */
class MediaDriver {
public:
  /**
   * Create a Media Driver with default configuration.
   */
  static std::unique_ptr<MediaDriver> launch();

  /**
   * Create a Media Driver with custom configuration.
   */
  static std::unique_ptr<MediaDriver> launch(const MediaDriverContext &context);

  /**
   * Constructor.
   */
  explicit MediaDriver(const MediaDriverContext &context);

  /**
   * Destructor.
   */
  ~MediaDriver();

  // Non-copyable, non-movable
  MediaDriver(const MediaDriver &) = delete;
  MediaDriver &operator=(const MediaDriver &) = delete;
  MediaDriver(MediaDriver &&) = delete;
  MediaDriver &operator=(MediaDriver &&) = delete;

  /**
   * Start the Media Driver.
   * This will start all component threads.
   */
  void start();

  /**
   * Stop the Media Driver.
   * This will gracefully shutdown all component threads.
   */
  void stop();

  /**
   * Check if the Media Driver is running.
   */
  bool is_running() const { return running_.load(); }

  /**
   * Get the driver context.
   */
  const MediaDriverContext &context() const { return context_; }

  /**
   * Get the conductor component.
   */
  Conductor &conductor() { return *conductor_; }

  /**
   * Get the sender component.
   */
  Sender &sender() { return *sender_; }

  /**
   * Get the receiver component.
   */
  Receiver &receiver() { return *receiver_; }

  /**
   * Get the counters manager.
   */
  CountersManager &counters_manager() { return *counters_manager_; }

  /**
   * Wait for the driver to terminate.
   */
  void wait_for_termination();

private:
  /**
   * Initialize the driver directory structure.
   */
  void init_driver_directory();

  /**
   * Cleanup driver resources.
   */
  void cleanup_driver_directory();

  /**
   * Setup signal handlers for graceful shutdown.
   */
  void setup_signal_handlers();

  MediaDriverContext context_;
  std::atomic<bool> running_{false};

  // Core components
  std::unique_ptr<CountersManager> counters_manager_;
  std::unique_ptr<Conductor> conductor_;
  std::unique_ptr<Sender> sender_;
  std::unique_ptr<Receiver> receiver_;

  // Shutdown coordination
  std::atomic<bool> shutdown_requested_{false};
};

} // namespace driver
} // namespace aeron
