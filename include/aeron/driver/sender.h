#pragma once

#include "../util/network_types.h"
#include "media_driver.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// Forward declarations for socket types
struct sockaddr;

namespace aeron {
namespace driver {

// Forward declarations
class SendChannelEndpoint;
class NetworkPublication;
class RetransmitHandler;
class DriverConductorProxy;

/**
 * Sender agent for handling outgoing network traffic.
 * Processes publications and sends UDP packets.
 */
class Sender : public MediaDriver::AgentRunner::Agent {
public:
  /**
   * Constructor for Sender.
   */
  explicit Sender(const DriverContext &context);

  /**
   * Destructor.
   */
  ~Sender() override;

  /**
   * Do work - process publications and send packets.
   */
  int do_work() override;

  /**
   * Start the sender.
   */
  void on_start() override;

  /**
   * Close the sender and clean up resources.
   */
  void on_close() override;

  /**
   * Get the role name for this agent.
   */
  const std::string &role_name() const override;

  // ========== Channel Management ==========

  /**
   * Add a send channel endpoint.
   */
  void add_endpoint(std::shared_ptr<SendChannelEndpoint> endpoint);

  /**
   * Remove a send channel endpoint.
   */
  void remove_endpoint(std::shared_ptr<SendChannelEndpoint> endpoint);

  /**
   * Add a network publication to an endpoint.
   */
  void add_network_publication(std::shared_ptr<NetworkPublication> publication);

  /**
   * Remove a network publication from an endpoint.
   */
  void
  remove_network_publication(std::shared_ptr<NetworkPublication> publication);

  // ========== Flow Control ==========

  /**
   * Process status message from a receiver.
   */
  void on_status_message(std::int32_t session_id, std::int32_t stream_id,
                         std::int64_t position, std::int32_t receiver_window,
                         const std::string &receiver_address);

  /**
   * Process NAK (negative acknowledgment) from a receiver.
   */
  void on_nak_message(std::int32_t session_id, std::int32_t stream_id,
                      std::int32_t term_id, std::int32_t term_offset,
                      std::int32_t length, const std::string &receiver_address);

  // ========== Statistics ==========

  /**
   * Get the number of packets sent.
   */
  std::int64_t packets_sent() const { return packets_sent_.load(); }

  /**
   * Get the number of bytes sent.
   */
  std::int64_t bytes_sent() const { return bytes_sent_.load(); }

  /**
   * Get the number of retransmissions.
   */
  std::int64_t retransmissions() const { return retransmissions_.load(); }

private:
  const DriverContext &context_;
  std::string role_name_;

  // Channel endpoints
  std::vector<std::shared_ptr<SendChannelEndpoint>> endpoints_;

  // Retransmission handling
  std::unique_ptr<RetransmitHandler> retransmit_handler_;

  // Communication with conductor
  std::unique_ptr<DriverConductorProxy> conductor_proxy_;

  // Statistics
  std::atomic<std::int64_t> packets_sent_;
  std::atomic<std::int64_t> bytes_sent_;
  std::atomic<std::int64_t> retransmissions_;

  // Work tracking
  std::int64_t total_work_done_;

  // Helper methods
  int process_endpoints();
  int process_retransmits();
  void send_data_packets();
  void send_setup_packets();
  void send_heartbeat_packets();
};

/**
 * Send channel endpoint for UDP communication.
 */
class SendChannelEndpoint {
public:
  /**
   * Constructor for SendChannelEndpoint.
   */
  SendChannelEndpoint(const std::string &channel, const DriverContext &context);

  /**
   * Destructor.
   */
  ~SendChannelEndpoint();

  /**
   * Get the socket file descriptor.
   */
  int socket_fd() const { return socket_fd_; }

  /**
   * Get the local address.
   */
  const util::NetworkAddress &local_address() const { return local_address_; }

  /**
   * Get the remote address.
   */
  const util::NetworkAddress &remote_address() const { return remote_address_; }

  /**
   * Get the channel string.
   */
  const std::string &channel() const { return channel_; }

  /**
   * Check if this endpoint is multicast.
   */
  bool is_multicast() const { return is_multicast_; }

  /**
   * Send a packet through this endpoint.
   */
  int send_packet(const std::uint8_t *buffer, std::size_t length,
                  const sockaddr *dest_addr = nullptr);

  /**
   * Add a network publication to this endpoint.
   */
  void add_network_publication(std::shared_ptr<NetworkPublication> publication);

  /**
   * Remove a network publication from this endpoint.
   */
  void
  remove_network_publication(std::shared_ptr<NetworkPublication> publication);

  /**
   * Get network publications for this endpoint.
   */
  const std::vector<std::shared_ptr<NetworkPublication>> &
  network_publications() const {
    return network_publications_;
  }

  /**
   * Process publications and send data.
   */
  int process_publications();

  /**
   * Close the endpoint.
   */
  void close();

  /**
   * Check if the endpoint is closed.
   */
  bool is_closed() const { return is_closed_.load(); }

private:
  std::string channel_;
  const DriverContext &context_;
  int socket_fd_;
  util::NetworkAddress local_address_;
  util::NetworkAddress remote_address_;
  bool is_multicast_;
  std::atomic<bool> is_closed_;

  // Connected publications
  std::vector<std::shared_ptr<NetworkPublication>> network_publications_;

  // Helper methods - implementation hidden
  void setup_socket();
  void parse_channel_uri();
  void configure_socket_buffers();
  void bind_socket();
  void connect_socket();
  void setup_multicast();
};

/**
 * Retransmit handler for managing NAK-based retransmissions.
 */
class RetransmitHandler {
public:
  /**
   * Constructor for RetransmitHandler.
   */
  explicit RetransmitHandler(const DriverContext &context);

  /**
   * Destructor.
   */
  ~RetransmitHandler();

  /**
   * Process a NAK and schedule retransmission.
   */
  void on_nak(std::int32_t session_id, std::int32_t stream_id,
              std::int32_t term_id, std::int32_t term_offset,
              std::int32_t length, const std::string &receiver_address,
              std::shared_ptr<NetworkPublication> publication);

  /**
   * Process scheduled retransmissions.
   * Returns the number of retransmissions sent.
   */
  int process_retransmissions();

  /**
   * Clean up old retransmission entries.
   */
  void clean_up_old_entries();

private:
  // Implementation details hidden
  class Impl;
  std::unique_ptr<Impl> impl_;
};

/**
 * Flow control interface for managing publication rates.
 */
class FlowControl {
public:
  /**
   * Virtual destructor.
   */
  virtual ~FlowControl() = default;

  /**
   * Update flow control state based on status message.
   */
  virtual void on_status_message(std::int64_t position,
                                 std::int32_t receiver_window,
                                 const std::string &receiver_address) = 0;

  /**
   * Get the current position limit for sending.
   */
  virtual std::int64_t position_limit() const = 0;

  /**
   * Check if we should send a heartbeat.
   */
  virtual bool should_send_heartbeat() const = 0;
};

/**
 * Max flow control - send as fast as possible.
 */
class MaxFlowControl : public FlowControl {
public:
  /**
   * Constructor.
   */
  explicit MaxFlowControl(std::int64_t initial_position_limit);

  /**
   * Update flow control state (no-op for max flow control).
   */
  void on_status_message(std::int64_t position, std::int32_t receiver_window,
                         const std::string &receiver_address) override;

  /**
   * Get the position limit (maximum value).
   */
  std::int64_t position_limit() const override;

  /**
   * Always send heartbeats in max flow control.
   */
  bool should_send_heartbeat() const override { return true; }

private:
  std::int64_t position_limit_;
};

/**
 * Min flow control - send at the rate of the slowest receiver.
 */
class MinFlowControl : public FlowControl {
public:
  /**
   * Constructor.
   */
  explicit MinFlowControl(std::int64_t initial_position_limit);

  /**
   * Update flow control state based on slowest receiver.
   */
  void on_status_message(std::int64_t position, std::int32_t receiver_window,
                         const std::string &receiver_address) override;

  /**
   * Get the position limit (minimum of all receivers).
   */
  std::int64_t position_limit() const override;

  /**
   * Send heartbeats when needed.
   */
  bool should_send_heartbeat() const override;

private:
  // Implementation details hidden
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace driver
} // namespace aeron