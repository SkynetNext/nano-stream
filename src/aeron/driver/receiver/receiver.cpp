#ifdef _WIN32
#define NOMINMAX
#pragma comment(lib, "ws2_32.lib")
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "aeron/driver/receiver/receiver.h"
#include <cstring>
#include <iostream>

namespace aeron {
namespace driver {

bool FragmentAssembler::add_fragment(const protocol::DataHeader &header,
                                     const std::uint8_t *data,
                                     std::int32_t length) {
  std::lock_guard<std::mutex> lock(fragments_mutex_);

  std::int64_t key = (static_cast<std::int64_t>(header.session_id) << 32) |
                     static_cast<std::uint32_t>(header.stream_id);

  auto it = fragments_.find(key);
  if (it == fragments_.end()) {
    // New fragment
    auto fragment = std::make_unique<Fragment>();
    fragment->session_id = header.session_id;
    fragment->stream_id = header.stream_id;
    fragment->term_id = header.term_id;
    fragment->term_offset = header.term_offset;
    fragment->timestamp = std::chrono::steady_clock::now();
    fragment->is_complete = false;

    fragment->data.resize(length);
    std::memcpy(fragment->data.data(), data, length);

    if (header.is_begin_fragment() && header.is_end_fragment()) {
      fragment->is_complete = true;
    }

    fragments_[key] = std::move(fragment);
    return fragments_[key]->is_complete;
  } else {
    // Append to existing fragment
    auto &fragment = it->second;
    size_t old_size = fragment->data.size();
    fragment->data.resize(old_size + length);
    std::memcpy(fragment->data.data() + old_size, data, length);

    if (header.is_end_fragment()) {
      fragment->is_complete = true;
    }

    return fragment->is_complete;
  }
}

std::unique_ptr<FragmentAssembler::Fragment>
FragmentAssembler::get_completed_message(std::int32_t session_id,
                                         std::int32_t stream_id) {
  std::lock_guard<std::mutex> lock(fragments_mutex_);

  std::int64_t key = (static_cast<std::int64_t>(session_id) << 32) |
                     static_cast<std::uint32_t>(stream_id);

  auto it = fragments_.find(key);
  if (it != fragments_.end() && it->second->is_complete) {
    auto fragment = std::move(it->second);
    fragments_.erase(it);
    return fragment;
  }

  return nullptr;
}

void FragmentAssembler::cleanup_old_fragments() {
  std::lock_guard<std::mutex> lock(fragments_mutex_);

  auto now = std::chrono::steady_clock::now();
  auto it = fragments_.begin();

  while (it != fragments_.end()) {
    if (std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->second->timestamp)
            .count() >= FRAGMENT_TIMEOUT_MS) {
      it = fragments_.erase(it);
    } else {
      ++it;
    }
  }
}

Receiver::Receiver() {
#ifdef _WIN32
  WSADATA wsaData;
  int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  winsock_initialized_ = (result == 0);
  if (!winsock_initialized_) {
    std::cerr << "WSAStartup failed: " << result << std::endl;
  }
#endif
}

Receiver::~Receiver() {
  stop();

#ifdef _WIN32
  if (winsock_initialized_) {
    WSACleanup();
  }
#endif
}

void Receiver::start() {
  if (running_.load()) {
    return;
  }

  running_.store(true);
  receiver_thread_ = std::thread(&Receiver::run, this);
  std::cout << "Receiver started" << std::endl;
}

void Receiver::stop() {
  if (!running_.load()) {
    return;
  }

  running_.store(false);
  if (receiver_thread_.joinable()) {
    receiver_thread_.join();
  }

  // Close all sockets
  std::lock_guard<std::mutex> lock(endpoints_mutex_);
  for (auto &pair : endpoints_) {
    auto &endpoint = pair.second;
#ifdef _WIN32
    if (endpoint->socket_fd != INVALID_SOCKET) {
      closesocket(endpoint->socket_fd);
    }
#else
    if (endpoint->socket_fd != INVALID_SOCKET) {
      close(endpoint->socket_fd);
    }
#endif
  }
  endpoints_.clear();

  std::cout << "Receiver stopped" << std::endl;
}

void Receiver::run() {
  while (running_.load()) {
    try {
      // Process inbound data from all subscriptions
      process_subscriptions();

      // Clean up old fragments
      fragment_assembler_.cleanup_old_fragments();

      // Sleep for a short duration
      std::this_thread::sleep_for(
          std::chrono::milliseconds(RECEIVER_TICK_DURATION_MS));

    } catch (const std::exception &e) {
      std::cerr << "Receiver error: " << e.what() << std::endl;
    }
  }
}

void Receiver::add_subscription(std::int32_t stream_id,
                                const std::string &channel) {
  std::lock_guard<std::mutex> lock(endpoints_mutex_);

  auto it = endpoints_.find(stream_id);
  if (it != endpoints_.end()) {
    return; // Already exists
  }

  auto endpoint = std::make_unique<ReceiveEndpoint>();
  endpoint->stream_id = stream_id;
  endpoint->channel = channel;

  if (parse_channel_uri(channel, *endpoint) && create_socket(*endpoint)) {
    endpoints_[stream_id] = std::move(endpoint);
    std::cout << "Added receive endpoint: stream=" << stream_id
              << ", channel=" << channel << std::endl;
  } else {
    std::cerr << "Failed to create receive endpoint for channel: " << channel
              << std::endl;
  }
}

void Receiver::remove_subscription(std::int32_t stream_id,
                                   const std::string &channel) {
  std::lock_guard<std::mutex> lock(endpoints_mutex_);

  auto it = endpoints_.find(stream_id);
  if (it != endpoints_.end()) {
#ifdef _WIN32
    if (it->second->socket_fd != INVALID_SOCKET) {
      closesocket(it->second->socket_fd);
    }
#else
    if (it->second->socket_fd != INVALID_SOCKET) {
      close(it->second->socket_fd);
    }
#endif
    endpoints_.erase(it);
    std::cout << "Removed receive endpoint: stream=" << stream_id << std::endl;
  }
}

void Receiver::process_subscriptions() {
  std::lock_guard<std::mutex> lock(endpoints_mutex_);

  for (auto &pair : endpoints_) {
    auto &endpoint = pair.second;
    receive_subscription_data(*endpoint);
  }
}

void Receiver::receive_subscription_data(ReceiveEndpoint &endpoint) {
  std::uint8_t buffer[MAX_UDP_PAYLOAD_LENGTH];
  struct sockaddr_in sender_addr;

#ifdef _WIN32
  int addr_len = sizeof(sender_addr);
  int bytes_received = recvfrom(
      endpoint.socket_fd, reinterpret_cast<char *>(buffer), sizeof(buffer), 0,
      reinterpret_cast<sockaddr *>(&sender_addr), &addr_len);

  if (bytes_received == SOCKET_ERROR) {
    int error = WSAGetLastError();
    if (error != WSAEWOULDBLOCK) {
      std::cerr << "recvfrom failed: " << error << std::endl;
    }
    return;
  }
#else
  socklen_t addr_len = sizeof(sender_addr);
  ssize_t bytes_received =
      recvfrom(endpoint.socket_fd, buffer, sizeof(buffer), MSG_DONTWAIT,
               reinterpret_cast<sockaddr *>(&sender_addr), &addr_len);

  if (bytes_received < 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      std::cerr << "recvfrom failed: " << strerror(errno) << std::endl;
    }
    return;
  }
#endif

  if (bytes_received < static_cast<int>(sizeof(protocol::DataHeader))) {
    return; // Too small to be valid
  }

  // Parse header
  const protocol::DataHeader *header =
      reinterpret_cast<const protocol::DataHeader *>(buffer);

  if (header->stream_id == endpoint.stream_id) {
    const std::uint8_t *data = buffer + protocol::DataHeader::HEADER_LENGTH;
    std::int32_t data_length =
        bytes_received - protocol::DataHeader::HEADER_LENGTH;

    process_data_frame(*header, data, data_length);
  }
}

void Receiver::process_data_frame(const protocol::DataHeader &header,
                                  const std::uint8_t *data,
                                  std::int32_t length) {
  // Add fragment to assembler
  bool is_complete = fragment_assembler_.add_fragment(header, data, length);

  if (is_complete) {
    auto fragment = fragment_assembler_.get_completed_message(header.session_id,
                                                              header.stream_id);
    if (fragment) {
      // Process complete message
      std::cout << "Received complete message: session=" << fragment->session_id
                << ", stream=" << fragment->stream_id
                << ", length=" << fragment->data.size() << std::endl;

      // In a real implementation, this would forward the message to subscribers
    }
  }

  // Update flow control
  {
    std::lock_guard<std::mutex> lock(flow_control_mutex_);
    std::int64_t key = (static_cast<std::int64_t>(header.session_id) << 32) |
                       static_cast<std::uint32_t>(header.stream_id);

    auto &state = flow_control_[key];
    state.last_term_id = header.term_id;
    state.last_term_offset = header.term_offset;

    // Send status message periodically
    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(
            now - state.last_status_message_time)
            .count() >= STATUS_MESSAGE_INTERVAL_MS) {
      send_status_message(header.session_id, header.stream_id, header.term_id,
                          header.term_offset);
      state.last_status_message_time = now;
    }
  }
}

void Receiver::process_setup_frame(const protocol::SetupFrame &setup_frame) {
  std::cout << "Received setup frame: session=" << setup_frame.header.session_id
            << ", stream=" << setup_frame.header.stream_id << std::endl;

  // In a real implementation, this would initialize the session state
}

void Receiver::send_status_message(std::int32_t session_id,
                                   std::int32_t stream_id, std::int32_t term_id,
                                   std::int32_t term_offset) {
  protocol::StatusMessageFrame status;
  status.init(session_id, stream_id);
  status.consumption_term_id = term_id;
  status.consumption_term_offset = term_offset;
  status.receiver_window_length = 128 * 1024; // 128KB window

  // In a real implementation, this would send the status message back to the
  // sender For now, just log it
  std::cout << "Sending status message: session=" << session_id
            << ", stream=" << stream_id << ", term_offset=" << term_offset
            << std::endl;
}

bool Receiver::create_socket(ReceiveEndpoint &endpoint) {
#ifdef _WIN32
  endpoint.socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (endpoint.socket_fd == INVALID_SOCKET) {
    std::cerr << "Failed to create socket: " << WSAGetLastError() << std::endl;
    return false;
  }

  // Set non-blocking mode
  u_long mode = 1;
  if (ioctlsocket(endpoint.socket_fd, FIONBIO, &mode) != 0) {
    std::cerr << "Failed to set non-blocking mode: " << WSAGetLastError()
              << std::endl;
    closesocket(endpoint.socket_fd);
    return false;
  }
#else
  endpoint.socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (endpoint.socket_fd == INVALID_SOCKET) {
    std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
    return false;
  }

  // Set non-blocking mode
  int flags = fcntl(endpoint.socket_fd, F_GETFL, 0);
  if (fcntl(endpoint.socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    std::cerr << "Failed to set non-blocking mode: " << strerror(errno)
              << std::endl;
    close(endpoint.socket_fd);
    return false;
  }
#endif

  // Bind to address
  if (bind(endpoint.socket_fd,
           reinterpret_cast<const sockaddr *>(&endpoint.bind_addr),
           sizeof(endpoint.bind_addr)) < 0) {
#ifdef _WIN32
    std::cerr << "Failed to bind socket: " << WSAGetLastError() << std::endl;
    closesocket(endpoint.socket_fd);
#else
    std::cerr << "Failed to bind socket: " << strerror(errno) << std::endl;
    close(endpoint.socket_fd);
#endif
    return false;
  }

  return true;
}

bool Receiver::parse_channel_uri(const std::string &channel,
                                 ReceiveEndpoint &endpoint) {
  // Simple URI parsing for "aeron:udp?endpoint=host:port"
  if (channel.find("aeron:udp?endpoint=") != 0) {
    std::cerr << "Unsupported channel URI: " << channel << std::endl;
    return false;
  }

  std::string endpoint_str = channel.substr(19); // Skip "aeron:udp?endpoint="
  size_t colon_pos = endpoint_str.find(':');

  if (colon_pos == std::string::npos) {
    std::cerr << "Invalid endpoint format: " << endpoint_str << std::endl;
    return false;
  }

  std::string host = endpoint_str.substr(0, colon_pos);
  std::string port_str = endpoint_str.substr(colon_pos + 1);

  int port = std::stoi(port_str);

  // Setup bind address
  std::memset(&endpoint.bind_addr, 0, sizeof(endpoint.bind_addr));
  endpoint.bind_addr.sin_family = AF_INET;
  endpoint.bind_addr.sin_port = htons(static_cast<std::uint16_t>(port));

  if (host == "localhost" || host == "127.0.0.1") {
    endpoint.bind_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  } else {
    endpoint.bind_addr.sin_addr.s_addr = INADDR_ANY; // Bind to all interfaces
  }

  // Check if it's multicast
  std::uint32_t addr = ntohl(endpoint.bind_addr.sin_addr.s_addr);
  endpoint.is_multicast = (addr >= 0xE0000000 && addr <= 0xEFFFFFFF);

  return true;
}

void Receiver::check_for_gaps(std::int32_t session_id, std::int32_t stream_id,
                              std::int32_t term_id, std::int32_t term_offset) {
  // Placeholder implementation for gap detection
  // In a real implementation, this would:
  // 1. Track expected sequence numbers
  // 2. Detect missing packets
  // 3. Send NAK (Negative Acknowledgment) messages for retransmission
}

} // namespace driver
} // namespace aeron
