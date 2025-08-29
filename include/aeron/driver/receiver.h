#pragma once

#include "../util/network_types.h"
#include "media_driver.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Forward declarations for socket types
struct sockaddr;
struct sockaddr_storage;

namespace aeron {
namespace driver {

// Forward declarations
class ReceiveChannelEndpoint;
class PublicationImage;
class DataPacketDispatcher;
class DriverConductorProxy;

/**
 * Receiver agent for handling incoming network traffic.
 * Processes UDP packets and manages receive channel endpoints.
 */
class Receiver : public MediaDriver::AgentRunner::Agent {
public:
  /**
   * Constructor for Receiver.
   */
  explicit Receiver(const DriverContext &context);

  /**
   * Destructor.
   */
  ~Receiver() override;

  /**
   * Do work - poll for incoming packets and process them.
   */
  int do_work() override;

  /**
   * Start the receiver.
   */
  void on_start() override;

  /**
   * Close the receiver and clean up resources.
   */
  void on_close() override;

  /**
   * Get the role name for this agent.
   */
  const std::string &role_name() const override;

  // ========== Channel Management ==========

  /**
   * Add a receive channel endpoint.
   */
  void add_endpoint(std::shared_ptr<ReceiveChannelEndpoint> endpoint);

  /**
   * Remove a receive channel endpoint.
   */
  void remove_endpoint(std::shared_ptr<ReceiveChannelEndpoint> endpoint);

  /**
   * Add a publication image to an endpoint.
   */
  void add_publication_image(std::shared_ptr<PublicationImage> image);

  /**
   * Remove a publication image from an endpoint.
   */
  void remove_publication_image(std::shared_ptr<PublicationImage> image);

  // ========== Statistics ==========

  /**
   * Get the number of packets received.
   */
  std::int64_t packets_received() const { return packets_received_.load(); }

  /**
   * Get the number of bytes received.
   */
  std::int64_t bytes_received() const { return bytes_received_.load(); }

  /**
   * Get the number of invalid packets.
   */
  std::int64_t invalid_packets() const { return invalid_packets_.load(); }

private:
  const DriverContext &context_;
  std::string role_name_;

  // Transport polling
  std::unique_ptr<class DataTransportPoller> data_transport_poller_;
  std::unique_ptr<DataPacketDispatcher> dispatcher_;

  // Channel endpoints
  std::vector<std::shared_ptr<ReceiveChannelEndpoint>> endpoints_;

  // Communication with conductor
  std::unique_ptr<DriverConductorProxy> conductor_proxy_;

  // Statistics
  std::atomic<std::int64_t> packets_received_;
  std::atomic<std::int64_t> bytes_received_;
  std::atomic<std::int64_t> invalid_packets_;

  // Work tracking
  std::int64_t total_work_done_;

  // Helper methods
  int poll_transports();
  void process_packet(const std::uint8_t *buffer, std::size_t length,
                      const sockaddr *src_addr);
};

/**
 * Data transport poller for UDP sockets.
 */
class DataTransportPoller {
public:
  /**
   * Constructor for DataTransportPoller.
   */
  explicit DataTransportPoller(const DriverContext &context);

  /**
   * Destructor.
   */
  ~DataTransportPoller();

  /**
   * Add a receive channel endpoint to polling.
   */
  void add_endpoint(std::shared_ptr<ReceiveChannelEndpoint> endpoint);

  /**
   * Remove a receive channel endpoint from polling.
   */
  void remove_endpoint(std::shared_ptr<ReceiveChannelEndpoint> endpoint);

  /**
   * Poll for incoming data.
   * Returns the number of packets processed.
   */
  int poll(
      std::function<void(const std::uint8_t *, std::size_t, const sockaddr *)>
          packet_handler);

private:
  const DriverContext &context_;
  std::vector<std::shared_ptr<ReceiveChannelEndpoint>> endpoints_;

  // Platform-specific implementation details are hidden
  class Impl;
  std::unique_ptr<Impl> impl_;
};

/**
 * Receive channel endpoint for UDP communication.
 */
class ReceiveChannelEndpoint {
public:
  /**
   * Constructor for ReceiveChannelEndpoint.
   */
  ReceiveChannelEndpoint(const std::string &channel,
                         const DriverContext &context);

  /**
   * Destructor.
   */
  ~ReceiveChannelEndpoint();

  /**
   * Get the socket file descriptor.
   */
  int socket_fd() const { return socket_fd_; }

  /**
   * Get the local address.
   */
  const util::NetworkAddress &local_address() const { return local_address_; }

  /**
   * Get the channel string.
   */
  const std::string &channel() const { return channel_; }

  /**
   * Check if this endpoint is multicast.
   */
  bool is_multicast() const { return is_multicast_; }

  /**
   * Receive packets from this endpoint.
   */
  int receive_packets(
      std::function<void(const std::uint8_t *, std::size_t, const sockaddr *)>
          packet_handler);

  /**
   * Add a publication image to this endpoint.
   */
  void add_publication_image(std::shared_ptr<PublicationImage> image);

  /**
   * Remove a publication image from this endpoint.
   */
  void remove_publication_image(std::shared_ptr<PublicationImage> image);

  /**
   * Get publication images for this endpoint.
   */
  const std::vector<std::shared_ptr<PublicationImage>> &
  publication_images() const {
    return publication_images_;
  }

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
  bool is_multicast_;
  std::atomic<bool> is_closed_;

  // Connected images
  std::vector<std::shared_ptr<PublicationImage>> publication_images_;

  // Receive buffer
  static constexpr std::size_t RECEIVE_BUFFER_SIZE = 65536;
  std::uint8_t receive_buffer_[RECEIVE_BUFFER_SIZE];

  // Helper methods - implementation hidden
  void setup_socket();
  void parse_channel_uri();
  void configure_socket_buffers();
  void bind_socket();
  void join_multicast_group();
};

/**
 * Data packet dispatcher for routing packets to the correct images.
 */
class DataPacketDispatcher {
public:
  /**
   * Constructor for DataPacketDispatcher.
   */
  DataPacketDispatcher();

  /**
   * Destructor.
   */
  ~DataPacketDispatcher();

  /**
   * Add a publication image for packet dispatching.
   */
  void add_publication_image(std::shared_ptr<PublicationImage> image);

  /**
   * Remove a publication image from dispatching.
   */
  void remove_publication_image(std::shared_ptr<PublicationImage> image);

  /**
   * Dispatch a packet to the appropriate image.
   */
  void dispatch_packet(const std::uint8_t *buffer, std::size_t length,
                       const sockaddr *src_addr);

private:
  // Implementation details hidden
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace driver
} // namespace aeron