#pragma once

#include "../nano_stream/ring_buffer.h"
#include "concurrent/atomic_buffer.h"
#include "concurrent/broadcast_receiver.h"
#include "context.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <unordered_map>

namespace aeron {

// Forward declarations
class Aeron;
class Publication;
class ExclusivePublication;
class Subscription;
class Image;
class DriverProxy;
class DriverEventsAdapter;

/**
 * Client Conductor manages the client-side of Aeron communication.
 * It handles registration with the Media Driver and manages local resources.
 */
class ClientConductor {
public:
  /**
   * Constructor for ClientConductor.
   */
  explicit ClientConductor(const Context &context, Aeron &aeron)
      : context_(context), aeron_(aeron), running_(false), client_id_(0),
        correlation_id_counter_(0) {
    // Real constructor with proper Aeron reference
  }

  /**
   * Destructor.
   */
  ~ClientConductor();

  /**
   * Start the client conductor.
   */
  void start();

  /**
   * Stop the client conductor.
   */
  void stop() { running_.store(false); }

  /**
   * Main conductor loop (for threading).
   */
  void run() {
    running_.store(true);
    while (running_.load()) {
      do_work();
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  /**
   * Check if the conductor is running.
   */
  bool is_running() const { return running_.load(); }

  /**
   * Do work - process driver events and manage resources.
   */
  int do_work() {
    int work_count = 0;

    // Process driver events (simplified for now)
    if (driver_events_adapter_) {
      // work_count += driver_events_adapter_->process_events();
      work_count += 1; // Placeholder
    }

    // Process heartbeats and timeouts
    work_count += process_timeouts();

    // Clean up closed resources
    work_count += cleanup_resources();

    return work_count;
  }

  // ========== Publication Management ==========

  /**
   * Add a publication.
   */
  std::shared_ptr<Publication> add_publication(const std::string &channel,
                                               std::int32_t stream_id);

  /**
   * Add an exclusive publication.
   */
  std::shared_ptr<ExclusivePublication>
  add_exclusive_publication(const std::string &channel, std::int32_t stream_id);

  /**
   * Close a publication.
   */
  void close_publication(std::shared_ptr<Publication> publication);

  /**
   * Close an exclusive publication.
   */
  void close_publication(std::shared_ptr<ExclusivePublication> publication);

  // ========== Subscription Management ==========

  /**
   * Add a subscription.
   */
  std::shared_ptr<Subscription> add_subscription(const std::string &channel,
                                                 std::int32_t stream_id);

  /**
   * Add a subscription with image handlers.
   */
  std::shared_ptr<Subscription> add_subscription(
      const std::string &channel, std::int32_t stream_id,
      Context::available_image_handler_t available_image_handler,
      Context::unavailable_image_handler_t unavailable_image_handler);

  /**
   * Close a subscription.
   */
  void close_subscription(std::shared_ptr<Subscription> subscription);

  // ========== Driver Events ==========

  /**
   * Process publication ready event from driver.
   */
  void on_publication_ready(std::int64_t original_registration_id,
                            std::int64_t registration_id,
                            std::int32_t session_id, std::int32_t stream_id,
                            std::int32_t position_limit_counter_id,
                            const std::string &log_file_name);

  /**
   * Process subscription ready event from driver.
   */
  void on_subscription_ready(std::int64_t registration_id,
                             std::int32_t channel_status_indicator_id);

  /**
   * Process operation succeeded event from driver.
   */
  void on_operation_succeeded(std::int64_t correlation_id);

  /**
   * Process error response from driver.
   */
  void on_error_response(std::int64_t correlation_id, std::int32_t error_code,
                         const std::string &error_message);

  /**
   * Process available image event from driver.
   */
  void on_available_image(std::int64_t correlation_id, std::int32_t session_id,
                          std::int64_t subscription_registration_id,
                          std::int32_t subscriber_position_id,
                          const std::string &log_file_name,
                          const std::string &source_identity);

  /**
   * Process unavailable image event from driver.
   */
  void on_unavailable_image(std::int64_t correlation_id,
                            std::int64_t subscription_registration_id);

  // ========== Client Management ==========

  /**
   * Send keepalive to driver.
   */
  void send_keepalive();

  /**
   * Check for driver liveness.
   */
  void check_driver_liveness();

  /**
   * Get the client id.
   */
  std::int64_t client_id() const { return client_id_; }

  /**
   * Get the next correlation id.
   */
  std::int64_t next_correlation_id();

private:
  const Context &context_;
  Aeron &aeron_;
  std::atomic<bool> running_;
  std::int64_t client_id_;
  std::atomic<std::int64_t> correlation_id_counter_;

  // Driver communication
  std::unique_ptr<DriverProxy> driver_proxy_;
  std::unique_ptr<DriverEventsAdapter> driver_events_adapter_;

public:
  // Resource tracking
  struct PublicationStateDefn {
    std::int64_t original_registration_id;
    std::int64_t registration_id;
    std::string channel;
    std::int32_t stream_id;
    std::shared_ptr<Publication> publication;
    bool is_exclusive;
    enum State { AWAITING_DRIVER, READY, CLOSED } state;
  };

  struct SubscriptionStateDefn {
    std::int64_t registration_id;
    std::string channel;
    std::int32_t stream_id;
    std::shared_ptr<Subscription> subscription;
    Context::available_image_handler_t available_image_handler;
    Context::unavailable_image_handler_t unavailable_image_handler;
    enum State { AWAITING_DRIVER, READY, CLOSED } state;
  };

  std::unordered_map<std::int64_t, PublicationStateDefn> publications_;
  std::unordered_map<std::int64_t, SubscriptionStateDefn> subscriptions_;
  std::unordered_map<std::int64_t, std::shared_ptr<Image>> images_;

  // Timing
  std::chrono::steady_clock::time_point start_time_;
  std::int64_t last_keepalive_time_;
  std::int64_t last_driver_check_time_;
  std::int64_t driver_timeout_ns_;
  std::int64_t keepalive_interval_ns_;

  // Work tracking
  std::int64_t total_work_done_;

  // Helper methods
  std::int64_t nano_time() const;
  void validate_channel(const std::string &channel) const;
  void validate_stream_id(std::int32_t stream_id) const;

  // Work processing methods
  int process_timeouts() { return 0; }  // Simplified placeholder
  int cleanup_resources() { return 0; } // Simplified placeholder

  std::shared_ptr<Publication>
  create_publication(const PublicationStateDefn &state) const;
  std::shared_ptr<ExclusivePublication>
  create_exclusive_publication(const PublicationStateDefn &state) const;
  std::shared_ptr<Subscription>
  create_subscription(const SubscriptionStateDefn &state) const;
  std::shared_ptr<Image> create_image(std::int64_t correlation_id,
                                      std::int32_t session_id,
                                      const std::string &log_file_name,
                                      const std::string &source_identity) const;

  void link_image_to_subscription(std::shared_ptr<Image> image,
                                  std::int64_t subscription_registration_id);
  void
  unlink_image_from_subscription(std::int64_t correlation_id,
                                 std::int64_t subscription_registration_id);

  // Cleanup methods
  void clean_up_publications();
  void clean_up_subscriptions();
  void clean_up_images();

  // Error handling
  void handle_error(const std::exception &exception);
  void on_driver_timeout();
};

/**
 * Driver proxy for sending commands to the Media Driver.
 */
class DriverProxy {
public:
  /**
   * Constructor for DriverProxy.
   */
  explicit DriverProxy(const Context &context);

  /**
   * Destructor.
   */
  ~DriverProxy();

  /**
   * Add publication command.
   */
  void add_publication(const std::string &channel, std::int32_t stream_id,
                       std::int64_t correlation_id, std::int64_t client_id);

  /**
   * Remove publication command.
   */
  void remove_publication(std::int64_t registration_id,
                          std::int64_t correlation_id, std::int64_t client_id);

  /**
   * Add exclusive publication command.
   */
  void add_exclusive_publication(const std::string &channel,
                                 std::int32_t stream_id,
                                 std::int64_t correlation_id,
                                 std::int64_t client_id);

  /**
   * Add subscription command.
   */
  void add_subscription(const std::string &channel, std::int32_t stream_id,
                        std::int64_t correlation_id, std::int64_t client_id);

  /**
   * Remove subscription command.
   */
  void remove_subscription(std::int64_t registration_id,
                           std::int64_t correlation_id, std::int64_t client_id);

  /**
   * Send client keepalive.
   */
  void client_keepalive(std::int64_t client_id);

  /**
   * Send terminate driver command.
   */
  void terminate_driver();

private:
  const Context &context_;

  // Driver communication buffers and ring buffer
  std::unique_ptr<concurrent::AtomicBuffer> command_buffer_;
  std::unique_ptr<nano_stream::RingBuffer<uint8_t>> to_driver_ring_buffer_;

  // Command building helpers
  void write_command_header(std::int32_t msg_type_id, std::int32_t length);
  std::int32_t write_string(const std::string &str, std::int32_t offset);
};

/**
 * Driver events adapter for processing events from the Media Driver.
 */
class DriverEventsAdapter {
public:
  /**
   * Constructor for DriverEventsAdapter.
   */
  DriverEventsAdapter(const Context &context, ClientConductor &conductor);

  /**
   * Destructor.
   */
  ~DriverEventsAdapter();

  /**
   * Process driver events.
   * Returns the number of events processed.
   */
  int process_events();

private:
  const Context &context_;
  ClientConductor &conductor_;
  std::unique_ptr<concurrent::BroadcastReceiver> broadcast_receiver_;

  // Event processing methods
  void on_command(std::int32_t msg_type_id, concurrent::AtomicBuffer &buffer,
                  std::int32_t offset, std::int32_t length);

  void on_publication_ready(concurrent::AtomicBuffer &buffer,
                            std::int32_t offset);
  void on_subscription_ready(concurrent::AtomicBuffer &buffer,
                             std::int32_t offset);
  void on_operation_succeeded(concurrent::AtomicBuffer &buffer,
                              std::int32_t offset);
  void on_error_response(concurrent::AtomicBuffer &buffer, std::int32_t offset);
  void on_available_image(concurrent::AtomicBuffer &buffer,
                          std::int32_t offset);
  void on_unavailable_image(concurrent::AtomicBuffer &buffer,
                            std::int32_t offset);

  // Utility methods
  std::string read_string(concurrent::AtomicBuffer &buffer,
                          std::int32_t &offset);
};

} // namespace aeron
