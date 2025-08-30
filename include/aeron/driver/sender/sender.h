#pragma once

#include "aeron/driver/log_buffer_manager.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_ // Prevent winsock.h from being included
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
typedef int SOCKET;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#endif

namespace aeron {
namespace driver {

/**
 * Network endpoint for sending data.
 */
struct SendEndpoint {
  std::int32_t session_id;
  std::int32_t stream_id;
  std::string channel;

#ifdef _WIN32
  SOCKET socket_fd;
#else
  int socket_fd;
#endif

  struct sockaddr_in destination_addr;
  bool is_multicast;
  std::int32_t mtu_length;

  // Flow control
  std::int32_t term_length;
  std::int32_t term_id;
  std::int32_t term_offset;
  std::int64_t position;

  SendEndpoint()
      : socket_fd(INVALID_SOCKET), is_multicast(false), mtu_length(1408),
        term_length(64 * 1024), term_id(0), term_offset(0), position(0) {}
};

/**
 * The Sender thread handles outbound network traffic.
 * It reads from publication buffers and sends UDP packets.
 */
class Sender {
public:
  /**
   * Constructor.
   */
  Sender();

  /**
   * Destructor.
   */
  ~Sender();

  /**
   * Start the sender thread.
   */
  void start();

  /**
   * Stop the sender thread.
   */
  void stop();

  /**
   * Check if the sender is running.
   */
  bool is_running() const { return running_.load(); }

  /**
   * Main sender loop (runs in separate thread).
   */
  void run();

  /**
   * Add a new publication endpoint.
   */
  void add_publication(std::int32_t session_id, std::int32_t stream_id,
                       const std::string &channel);

  /**
   * Remove a publication endpoint.
   */
  void remove_publication(std::int32_t session_id, std::int32_t stream_id);

  /**
   * Set the log buffer manager.
   */
  void set_log_buffer_manager(std::shared_ptr<LogBufferManager> manager);

private:
  /**
   * Process outbound data from all publications.
   */
  void process_publications();

  /**
   * Send data for a specific publication.
   */
  void send_publication_data(
      SendEndpoint &endpoint,
      std::shared_ptr<LogBufferManager::PublicationInfo> publication);

  /**
   * Send a single data frame.
   */
  void send_data_frame(SendEndpoint &endpoint, const std::uint8_t *data,
                       std::int32_t length);

  /**
   * Send setup frame for new connections.
   */
  void send_setup_frame(SendEndpoint &endpoint);

  /**
   * Handle retransmission requests.
   */
  void process_retransmits();

  /**
   * Create UDP socket for endpoint.
   */
  bool create_socket(SendEndpoint &endpoint);

  /**
   * Parse channel URI and setup endpoint.
   */
  bool parse_channel_uri(const std::string &channel, SendEndpoint &endpoint);

  /**
   * Fragment large messages into MTU-sized packets.
   */
  void send_fragmented_message(SendEndpoint &endpoint, const std::uint8_t *data,
                               std::int32_t length);

  // Thread control
  std::atomic<bool> running_{false};
  std::thread sender_thread_;

  // Publication endpoints
  std::mutex endpoints_mutex_;
  std::unordered_map<std::int64_t, std::unique_ptr<SendEndpoint>>
      endpoints_; // key: (session_id << 32) | stream_id

  // Retransmission tracking
  struct RetransmitRequest {
    std::int32_t session_id;
    std::int32_t stream_id;
    std::int32_t term_id;
    std::int32_t term_offset;
    std::int32_t length;
    std::chrono::steady_clock::time_point timestamp;
  };

  std::vector<RetransmitRequest> retransmit_requests_;
  std::mutex retransmit_mutex_;

  // Log buffer manager
  std::shared_ptr<LogBufferManager> log_buffer_manager_;

  // Configuration
  static constexpr std::int32_t SENDER_TICK_DURATION_MS = 1;
  static constexpr std::int32_t MAX_UDP_PAYLOAD_LENGTH = 1408;
  static constexpr std::int32_t RETRANSMIT_TIMEOUT_MS = 100;

#ifdef _WIN32
  bool winsock_initialized_;
#endif
};

} // namespace driver
} // namespace aeron
