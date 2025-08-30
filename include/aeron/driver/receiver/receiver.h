#pragma once

#include "../../protocol/header.h"
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
 * Network endpoint for receiving data.
 */
struct ReceiveEndpoint {
  std::int32_t stream_id;
  std::string channel;

#ifdef _WIN32
  SOCKET socket_fd;
#else
  int socket_fd;
#endif

  struct sockaddr_in bind_addr;
  bool is_multicast;
  std::int32_t mtu_length;

  // Reception tracking
  std::unordered_map<std::int32_t, std::int32_t>
      session_positions; // session_id -> term_offset

  ReceiveEndpoint()
      : socket_fd(INVALID_SOCKET), is_multicast(false), mtu_length(1408) {}
};

/**
 * Fragment assembler for reconstructing messages from UDP packets.
 */
class FragmentAssembler {
public:
  struct Fragment {
    std::vector<std::uint8_t> data;
    std::int32_t session_id;
    std::int32_t stream_id;
    std::int32_t term_id;
    std::int32_t term_offset;
    std::chrono::steady_clock::time_point timestamp;
    bool is_complete;
  };

  /**
   * Add a fragment to the assembler.
   * Returns true if message is complete.
   */
  bool add_fragment(const protocol::DataHeader &header,
                    const std::uint8_t *data, std::int32_t length);

  /**
   * Get completed message if available.
   */
  std::unique_ptr<Fragment> get_completed_message(std::int32_t session_id,
                                                  std::int32_t stream_id);

  /**
   * Clean up old incomplete fragments.
   */
  void cleanup_old_fragments();

private:
  std::unordered_map<std::int64_t, std::unique_ptr<Fragment>>
      fragments_; // key: (session_id << 32) | stream_id
  std::mutex fragments_mutex_;

  static constexpr std::int32_t FRAGMENT_TIMEOUT_MS = 1000;
};

/**
 * The Receiver thread handles inbound network traffic.
 * It receives UDP packets and assembles them into messages.
 */
class Receiver {
public:
  /**
   * Constructor.
   */
  Receiver();

  /**
   * Destructor.
   */
  ~Receiver();

  /**
   * Start the receiver thread.
   */
  void start();

  /**
   * Stop the receiver thread.
   */
  void stop();

  /**
   * Check if the receiver is running.
   */
  bool is_running() const { return running_.load(); }

  /**
   * Main receiver loop (runs in separate thread).
   */
  void run();

  /**
   * Add a new subscription endpoint.
   */
  void add_subscription(std::int32_t stream_id, const std::string &channel);

  /**
   * Remove a subscription endpoint.
   */
  void remove_subscription(std::int32_t stream_id, const std::string &channel);

private:
  /**
   * Process inbound data from all subscriptions.
   */
  void process_subscriptions();

  /**
   * Receive data for a specific subscription.
   */
  void receive_subscription_data(ReceiveEndpoint &endpoint);

  /**
   * Process received data frame.
   */
  void process_data_frame(const protocol::DataHeader &header,
                          const std::uint8_t *data, std::int32_t length);

  /**
   * Process setup frame.
   */
  void process_setup_frame(const protocol::SetupFrame &setup_frame);

  /**
   * Send status message (flow control).
   */
  void send_status_message(std::int32_t session_id, std::int32_t stream_id,
                           std::int32_t term_id, std::int32_t term_offset);

  /**
   * Create UDP socket for endpoint.
   */
  bool create_socket(ReceiveEndpoint &endpoint);

  /**
   * Parse channel URI and setup endpoint.
   */
  bool parse_channel_uri(const std::string &channel, ReceiveEndpoint &endpoint);

  /**
   * Check for missing packets and request retransmission.
   */
  void check_for_gaps(std::int32_t session_id, std::int32_t stream_id,
                      std::int32_t term_id, std::int32_t term_offset);

  // Thread control
  std::atomic<bool> running_{false};
  std::thread receiver_thread_;

  // Subscription endpoints
  std::mutex endpoints_mutex_;
  std::unordered_map<std::int32_t, std::unique_ptr<ReceiveEndpoint>>
      endpoints_; // key: stream_id

  // Fragment assembly
  FragmentAssembler fragment_assembler_;

  // Flow control
  struct FlowControlState {
    std::int32_t last_term_id;
    std::int32_t last_term_offset;
    std::chrono::steady_clock::time_point last_status_message_time;
  };

  std::unordered_map<std::int64_t, FlowControlState>
      flow_control_; // key: (session_id << 32) | stream_id
  std::mutex flow_control_mutex_;

  // Configuration
  static constexpr std::int32_t RECEIVER_TICK_DURATION_MS = 1;
  static constexpr std::int32_t MAX_UDP_PAYLOAD_LENGTH = 1408;
  static constexpr std::int32_t STATUS_MESSAGE_INTERVAL_MS = 100;
  static constexpr std::int32_t GAP_DETECTION_TIMEOUT_MS = 50;

#ifdef _WIN32
  bool winsock_initialized_;
#endif
};

} // namespace driver
} // namespace aeron
