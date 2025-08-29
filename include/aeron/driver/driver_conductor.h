#pragma once

#include "../log_buffers.h"
#include "media_driver.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace aeron {
namespace driver {

// Forward declarations
class ClientCommandAdapter;
class DriverProxy;
class ReceiverProxy;
class SenderProxy;
class IpcPublication;
class NetworkPublication;
class SubscriptionLink;
class PublicationImage;

/**
 * Publication types managed by the conductor.
 */
enum class PublicationType { IPC, NETWORK };

/**
 * Driver Conductor - coordinates all driver activities.
 * This is the central orchestrator that manages publications, subscriptions,
 * and communication with clients.
 */
class DriverConductor : public MediaDriver::AgentRunner::Agent {
public:
  /**
   * Constructor for DriverConductor.
   */
  explicit DriverConductor(const DriverContext &context);

  /**
   * Destructor.
   */
  ~DriverConductor() override;

  /**
   * Do work - process commands and manage resources.
   */
  int do_work() override;

  /**
   * Start the conductor.
   */
  void on_start() override;

  /**
   * Close the conductor and clean up resources.
   */
  void on_close() override;

  /**
   * Get the role name for this agent.
   */
  const std::string &role_name() const override;

  // ========== Client Command Processing ==========

  /**
   * Process add publication command.
   */
  void on_add_publication(const std::string &channel, std::int32_t stream_id,
                          std::int64_t correlation_id, std::int64_t client_id,
                          bool is_exclusive);

  /**
   * Process remove publication command.
   */
  void on_remove_publication(std::int64_t registration_id,
                             std::int64_t correlation_id,
                             std::int64_t client_id);

  /**
   * Process add subscription command.
   */
  void on_add_subscription(const std::string &channel, std::int32_t stream_id,
                           std::int64_t correlation_id, std::int64_t client_id);

  /**
   * Process remove subscription command.
   */
  void on_remove_subscription(std::int64_t registration_id,
                              std::int64_t correlation_id,
                              std::int64_t client_id);

  /**
   * Process client keepalive.
   */
  void on_client_keepalive(std::int64_t client_id);

  /**
   * Process client timeout.
   */
  void on_client_timeout(std::int64_t client_id);

  // ========== Network Event Processing ==========

  /**
   * Process create publication image command from receiver.
   */
  void on_create_publication_image(
      std::int32_t session_id, std::int32_t stream_id,
      std::int32_t initial_term_id, std::int32_t active_term_id,
      std::int32_t initial_term_offset, std::int32_t term_buffer_length,
      std::int32_t mtu_length, const std::string &source_address);

  /**
   * Process publication ready event.
   */
  void on_publication_ready(std::int64_t registration_id,
                            std::int64_t original_registration_id,
                            std::int32_t session_id, std::int32_t stream_id,
                            std::int32_t position_limit_counter_id);

  /**
   * Process subscription ready event.
   */
  void on_subscription_ready(std::int64_t registration_id,
                             std::int32_t channel_status_indicator_id);

  /**
   * Process operation succeeded event.
   */
  void on_operation_succeeded(std::int64_t correlation_id);

  /**
   * Process error response.
   */
  void on_error_response(std::int64_t correlation_id, std::int32_t error_code,
                         const std::string &error_message);

  // ========== Resource Management ==========

  /**
   * Clean up resources for a timed-out client.
   */
  void clean_up_client(std::int64_t client_id);

  /**
   * Check for timed-out resources.
   */
  void check_for_timeouts();

  /**
   * Get the current time in nanoseconds.
   */
  std::int64_t nano_time() const;

  /**
   * Get the current time in milliseconds.
   */
  std::int64_t epoch_time() const;

private:
  const DriverContext &context_;
  std::string role_name_;

  // Component proxies
  std::unique_ptr<class ClientCommandAdapter> client_command_adapter_;
  std::unique_ptr<class DriverProxy> driver_proxy_;
  std::unique_ptr<class ReceiverProxy> receiver_proxy_;
  std::unique_ptr<class SenderProxy> sender_proxy_;

  // Resource tracking
  std::atomic<std::int64_t> next_session_id_;
  std::atomic<std::int64_t> next_correlation_id_;

  // Publications management
  std::unordered_map<std::int64_t, std::shared_ptr<IpcPublication>>
      ipc_publications_;
  std::unordered_map<std::int64_t, std::shared_ptr<NetworkPublication>>
      network_publications_;

  // Subscriptions management
  std::unordered_map<std::int64_t, std::shared_ptr<SubscriptionLink>>
      subscription_links_;

  // Images management
  std::unordered_map<std::int64_t, std::shared_ptr<PublicationImage>> images_;

  // Client tracking
  struct ClientSession {
    std::int64_t client_id;
    std::int64_t time_of_last_keepalive;
    std::vector<std::int64_t> publication_links;
    std::vector<std::int64_t> subscription_links;
  };
  std::unordered_map<std::int64_t, ClientSession> client_sessions_;

  // Timing
  std::chrono::steady_clock::time_point start_time_;
  std::int64_t last_timeout_check_;

  // Helper methods
  std::int64_t generate_session_id();
  std::int64_t generate_correlation_id();

  std::shared_ptr<IpcPublication>
  create_ipc_publication(const std::string &channel, std::int32_t stream_id,
                         std::int64_t registration_id, bool is_exclusive);

  std::shared_ptr<NetworkPublication>
  create_network_publication(const std::string &channel, std::int32_t stream_id,
                             std::int64_t registration_id, bool is_exclusive);

  std::shared_ptr<SubscriptionLink>
  create_subscription_link(const std::string &channel, std::int32_t stream_id,
                           std::int64_t registration_id,
                           std::int64_t client_id);

  void link_subscription_to_images(std::shared_ptr<SubscriptionLink> link);
  void unlink_subscription_from_images(std::shared_ptr<SubscriptionLink> link);

  bool is_ipc_channel(const std::string &channel) const;
  bool is_network_channel(const std::string &channel) const;

  void validate_channel(const std::string &channel) const;
  void validate_stream_id(std::int32_t stream_id) const;

  // Client management
  void ensure_client_session(std::int64_t client_id);
  void update_client_keepalive(std::int64_t client_id);
  bool is_client_timed_out(const ClientSession &session) const;

  // Error handling
  void send_error_response(std::int64_t correlation_id, std::int32_t error_code,
                           const std::string &message, std::int64_t client_id);

  // Metrics and monitoring
  std::int64_t total_work_done_;
  std::int64_t client_timeouts_;
  std::int64_t publication_count_;
  std::int64_t subscription_count_;
};

/**
 * IPC Publication implementation.
 */
class IpcPublication {
public:
  /**
   * Constructor for IPC Publication.
   */
  IpcPublication(const std::string &channel, std::int32_t stream_id,
                 std::int32_t session_id, std::int64_t registration_id,
                 std::unique_ptr<LogBuffers> log_buffers, bool is_exclusive);

  /**
   * Destructor.
   */
  ~IpcPublication();

  /**
   * Get the registration id.
   */
  std::int64_t registration_id() const { return registration_id_; }

  /**
   * Get the session id.
   */
  std::int32_t session_id() const { return session_id_; }

  /**
   * Get the stream id.
   */
  std::int32_t stream_id() const { return stream_id_; }

  /**
   * Get the channel.
   */
  const std::string &channel() const { return channel_; }

  /**
   * Check if this is an exclusive publication.
   */
  bool is_exclusive() const { return is_exclusive_; }

  /**
   * Check if the publication is connected.
   */
  bool is_connected() const;

  /**
   * Get the log buffers.
   */
  LogBuffers &log_buffers() { return *log_buffers_; }

  /**
   * Update publisher position limit.
   */
  void update_publisher_limit();

  /**
   * Close the publication.
   */
  void close();

private:
  std::string channel_;
  std::int32_t stream_id_;
  std::int32_t session_id_;
  std::int64_t registration_id_;
  std::unique_ptr<LogBuffers> log_buffers_;
  bool is_exclusive_;
  std::atomic<bool> is_closed_;
};

/**
 * Network Publication implementation.
 */
class NetworkPublication {
public:
  /**
   * Constructor for Network Publication.
   */
  NetworkPublication(const std::string &channel, std::int32_t stream_id,
                     std::int32_t session_id, std::int64_t registration_id,
                     std::unique_ptr<LogBuffers> log_buffers,
                     bool is_exclusive);

  /**
   * Destructor.
   */
  ~NetworkPublication();

  /**
   * Get the registration id.
   */
  std::int64_t registration_id() const { return registration_id_; }

  /**
   * Get the session id.
   */
  std::int32_t session_id() const { return session_id_; }

  /**
   * Get the stream id.
   */
  std::int32_t stream_id() const { return stream_id_; }

  /**
   * Get the channel.
   */
  const std::string &channel() const { return channel_; }

  /**
   * Check if this is an exclusive publication.
   */
  bool is_exclusive() const { return is_exclusive_; }

  /**
   * Check if the publication is connected.
   */
  bool is_connected() const;

  /**
   * Get the log buffers.
   */
  LogBuffers &log_buffers() { return *log_buffers_; }

  /**
   * Send setup message to establish connection.
   */
  void send_setup_message();

  /**
   * Process heartbeat from subscribers.
   */
  void on_heartbeat(const std::string &source_address);

  /**
   * Close the publication.
   */
  void close();

private:
  std::string channel_;
  std::int32_t stream_id_;
  std::int32_t session_id_;
  std::int64_t registration_id_;
  std::unique_ptr<LogBuffers> log_buffers_;
  bool is_exclusive_;
  std::atomic<bool> is_closed_;

  // Network-specific state
  std::vector<std::string> connected_subscribers_;
  std::int64_t last_heartbeat_time_;
};

/**
 * Subscription Link implementation.
 */
class SubscriptionLink {
public:
  /**
   * Constructor for Subscription Link.
   */
  SubscriptionLink(const std::string &channel, std::int32_t stream_id,
                   std::int64_t registration_id, std::int64_t client_id);

  /**
   * Destructor.
   */
  ~SubscriptionLink();

  /**
   * Get the registration id.
   */
  std::int64_t registration_id() const { return registration_id_; }

  /**
   * Get the client id.
   */
  std::int64_t client_id() const { return client_id_; }

  /**
   * Get the stream id.
   */
  std::int32_t stream_id() const { return stream_id_; }

  /**
   * Get the channel.
   */
  const std::string &channel() const { return channel_; }

  /**
   * Check if this subscription matches the given stream.
   */
  bool matches(const std::string &channel, std::int32_t stream_id) const;

  /**
   * Add a connected image.
   */
  void add_image(std::shared_ptr<class PublicationImage> image);

  /**
   * Remove a connected image.
   */
  void remove_image(std::shared_ptr<class PublicationImage> image);

  /**
   * Get connected images.
   */
  const std::vector<std::shared_ptr<class PublicationImage>> &images() const {
    return images_;
  }

  /**
   * Close the subscription.
   */
  void close();

private:
  std::string channel_;
  std::int32_t stream_id_;
  std::int64_t registration_id_;
  std::int64_t client_id_;
  std::vector<std::shared_ptr<class PublicationImage>> images_;
  std::atomic<bool> is_closed_;
};

} // namespace driver
} // namespace aeron
