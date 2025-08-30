#ifdef _WIN32
#define NOMINMAX
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#endif

#include "aeron/driver/sender/sender.h"
#include "aeron/protocol/header.h"
#include <cstring>
#include <iostream>

namespace aeron {
namespace driver {

Sender::Sender() {
#ifdef _WIN32
  WSADATA wsaData;
  int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
  winsock_initialized_ = (result == 0);
  if (!winsock_initialized_) {
    std::cerr << "WSAStartup failed: " << result << std::endl;
  }
#endif
}

Sender::~Sender() {
  stop();

#ifdef _WIN32
  if (winsock_initialized_) {
    WSACleanup();
  }
#endif
}

void Sender::start() {
  if (running_.load()) {
    return;
  }

  running_.store(true);
  sender_thread_ = std::thread(&Sender::run, this);
  std::cout << "Sender started" << std::endl;
}

void Sender::stop() {
  if (!running_.load()) {
    return;
  }

  running_.store(false);
  if (sender_thread_.joinable()) {
    sender_thread_.join();
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

  std::cout << "Sender stopped" << std::endl;
}

void Sender::run() {
  while (running_.load()) {
    try {
      // Process outbound data from all publications
      process_publications();

      // Handle retransmission requests
      process_retransmits();

      // Sleep for a short duration
      std::this_thread::sleep_for(
          std::chrono::milliseconds(SENDER_TICK_DURATION_MS));

    } catch (const std::exception &e) {
      std::cerr << "Sender error: " << e.what() << std::endl;
    }
  }
}

void Sender::add_publication(std::int32_t session_id, std::int32_t stream_id,
                             const std::string &channel) {
  std::lock_guard<std::mutex> lock(endpoints_mutex_);

  std::int64_t key = (static_cast<std::int64_t>(session_id) << 32) |
                     static_cast<std::uint32_t>(stream_id);

  auto endpoint = std::make_unique<SendEndpoint>();
  endpoint->session_id = session_id;
  endpoint->stream_id = stream_id;
  endpoint->channel = channel;

  if (parse_channel_uri(channel, *endpoint) && create_socket(*endpoint)) {
    endpoints_[key] = std::move(endpoint);
    std::cout << "Added send endpoint: session=" << session_id
              << ", stream=" << stream_id << ", channel=" << channel
              << std::endl;
  } else {
    std::cerr << "Failed to create send endpoint for channel: " << channel
              << std::endl;
  }
}

void Sender::remove_publication(std::int32_t session_id,
                                std::int32_t stream_id) {
  std::lock_guard<std::mutex> lock(endpoints_mutex_);

  std::int64_t key = (static_cast<std::int64_t>(session_id) << 32) |
                     static_cast<std::uint32_t>(stream_id);

  auto it = endpoints_.find(key);
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
    std::cout << "Removed send endpoint: session=" << session_id
              << ", stream=" << stream_id << std::endl;
  }
}

void Sender::process_publications() {
  std::lock_guard<std::mutex> lock(endpoints_mutex_);

  for (auto &pair : endpoints_) {
    auto &endpoint = pair.second;
    send_publication_data(*endpoint);
  }
}

void Sender::send_publication_data(SendEndpoint &endpoint) {
  // Placeholder implementation
  // In a real implementation, this would:
  // 1. Read data from the publication's log buffer
  // 2. Fragment large messages if needed
  // 3. Send UDP packets with proper headers
  // 4. Update flow control state

  // For now, just send a setup frame periodically
  static auto last_setup_time = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();

  if (std::chrono::duration_cast<std::chrono::seconds>(now - last_setup_time)
          .count() >= 5) {
    send_setup_frame(endpoint);
    last_setup_time = now;
  }
}

void Sender::send_data_frame(SendEndpoint &endpoint, const std::uint8_t *data,
                             std::int32_t length) {
  if (length > MAX_UDP_PAYLOAD_LENGTH) {
    send_fragmented_message(endpoint, data, length);
    return;
  }

  // Create data header
  protocol::DataHeader header;
  header.init();
  header.session_id = endpoint.session_id;
  header.stream_id = endpoint.stream_id;
  header.term_id = endpoint.term_id;
  header.term_offset = endpoint.term_offset;
  header.set_frame_length(protocol::DataHeader::HEADER_LENGTH + length);
  header.set_begin_fragment();
  header.set_end_fragment();

  // Send header + data
  std::vector<std::uint8_t> packet(protocol::DataHeader::HEADER_LENGTH +
                                   length);
  std::memcpy(packet.data(), &header, protocol::DataHeader::HEADER_LENGTH);
  std::memcpy(packet.data() + protocol::DataHeader::HEADER_LENGTH, data,
              length);

#ifdef _WIN32
  int result =
      sendto(endpoint.socket_fd, reinterpret_cast<const char *>(packet.data()),
             static_cast<int>(packet.size()), 0,
             reinterpret_cast<const sockaddr *>(&endpoint.destination_addr),
             sizeof(endpoint.destination_addr));
#else
  ssize_t result =
      sendto(endpoint.socket_fd, packet.data(), packet.size(), 0,
             reinterpret_cast<const sockaddr *>(&endpoint.destination_addr),
             sizeof(endpoint.destination_addr));
#endif

  if (result < 0) {
    std::cerr << "Failed to send data frame" << std::endl;
  } else {
    endpoint.term_offset += length;
    endpoint.position += length;
  }
}

void Sender::send_setup_frame(SendEndpoint &endpoint) {
  protocol::SetupFrame setup;
  setup.init(endpoint.session_id, endpoint.stream_id);
  setup.term_length = endpoint.term_length;
  setup.mtu_length = endpoint.mtu_length;
  setup.initial_term_id = endpoint.term_id;
  setup.active_term_id = endpoint.term_id;
  setup.term_offset = endpoint.term_offset;

#ifdef _WIN32
  int result = sendto(
      endpoint.socket_fd, reinterpret_cast<const char *>(&setup), sizeof(setup),
      0, reinterpret_cast<const sockaddr *>(&endpoint.destination_addr),
      sizeof(endpoint.destination_addr));
#else
  ssize_t result =
      sendto(endpoint.socket_fd, &setup, sizeof(setup), 0,
             reinterpret_cast<const sockaddr *>(&endpoint.destination_addr),
             sizeof(endpoint.destination_addr));
#endif

  if (result < 0) {
    std::cerr << "Failed to send setup frame" << std::endl;
  }
}

void Sender::process_retransmits() {
  std::lock_guard<std::mutex> lock(retransmit_mutex_);

  auto now = std::chrono::steady_clock::now();
  auto it = retransmit_requests_.begin();

  while (it != retransmit_requests_.end()) {
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                              it->timestamp)
            .count() >= RETRANSMIT_TIMEOUT_MS) {
      // Handle retransmit request
      // In a real implementation, this would resend the requested data
      it = retransmit_requests_.erase(it);
    } else {
      ++it;
    }
  }
}

bool Sender::create_socket(SendEndpoint &endpoint) {
#ifdef _WIN32
  endpoint.socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (endpoint.socket_fd == INVALID_SOCKET) {
    std::cerr << "Failed to create socket: " << WSAGetLastError() << std::endl;
    return false;
  }
#else
  endpoint.socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (endpoint.socket_fd == INVALID_SOCKET) {
    std::cerr << "Failed to create socket: " << strerror(errno) << std::endl;
    return false;
  }
#endif

  // Set socket options for performance
  int opt = 1;
#ifdef _WIN32
  setsockopt(endpoint.socket_fd, SOL_SOCKET, SO_REUSEADDR,
             reinterpret_cast<const char *>(&opt), sizeof(opt));
#else
  setsockopt(endpoint.socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

  return true;
}

bool Sender::parse_channel_uri(const std::string &channel,
                               SendEndpoint &endpoint) {
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

  // Setup destination address
  std::memset(&endpoint.destination_addr, 0, sizeof(endpoint.destination_addr));
  endpoint.destination_addr.sin_family = AF_INET;
  endpoint.destination_addr.sin_port = htons(static_cast<std::uint16_t>(port));

  if (host == "localhost" || host == "127.0.0.1") {
    endpoint.destination_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  } else {
#ifdef _WIN32
    if (InetPtonA(AF_INET, host.c_str(), &endpoint.destination_addr.sin_addr) !=
        1) {
      std::cerr << "Invalid IP address: " << host << std::endl;
      return false;
    }
#else
    if (inet_pton(AF_INET, host.c_str(), &endpoint.destination_addr.sin_addr) !=
        1) {
      std::cerr << "Invalid IP address: " << host << std::endl;
      return false;
    }
#endif
  }

  // Check if it's multicast
  std::uint32_t addr = ntohl(endpoint.destination_addr.sin_addr.s_addr);
  endpoint.is_multicast = (addr >= 0xE0000000 && addr <= 0xEFFFFFFF);

  return true;
}

void Sender::send_fragmented_message(SendEndpoint &endpoint,
                                     const std::uint8_t *data,
                                     std::int32_t length) {
  std::int32_t remaining = length;
  std::int32_t offset = 0;
  std::int32_t fragment_size =
      MAX_UDP_PAYLOAD_LENGTH - protocol::DataHeader::HEADER_LENGTH;

  while (remaining > 0) {
    std::int32_t current_fragment_size = std::min(remaining, fragment_size);
    bool is_first = (offset == 0);
    bool is_last = (remaining == current_fragment_size);

    protocol::DataHeader header;
    header.init();
    header.session_id = endpoint.session_id;
    header.stream_id = endpoint.stream_id;
    header.term_id = endpoint.term_id;
    header.term_offset = endpoint.term_offset + offset;
    header.set_frame_length(protocol::DataHeader::HEADER_LENGTH +
                            current_fragment_size);

    if (is_first)
      header.set_begin_fragment();
    if (is_last)
      header.set_end_fragment();

    // Send fragment
    std::vector<std::uint8_t> packet(protocol::DataHeader::HEADER_LENGTH +
                                     current_fragment_size);
    std::memcpy(packet.data(), &header, protocol::DataHeader::HEADER_LENGTH);
    std::memcpy(packet.data() + protocol::DataHeader::HEADER_LENGTH,
                data + offset, current_fragment_size);

#ifdef _WIN32
    sendto(endpoint.socket_fd, reinterpret_cast<const char *>(packet.data()),
           static_cast<int>(packet.size()), 0,
           reinterpret_cast<const sockaddr *>(&endpoint.destination_addr),
           sizeof(endpoint.destination_addr));
#else
    sendto(endpoint.socket_fd, packet.data(), packet.size(), 0,
           reinterpret_cast<const sockaddr *>(&endpoint.destination_addr),
           sizeof(endpoint.destination_addr));
#endif

    offset += current_fragment_size;
    remaining -= current_fragment_size;
  }

  endpoint.term_offset += length;
  endpoint.position += length;
}

} // namespace driver
} // namespace aeron
