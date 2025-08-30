#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#endif

#include "aeron/driver/publication/network_publication.h"
#include "aeron/driver/sender/sender.h"
#include "aeron/logbuffer/log_buffer_descriptor.h"
#include "aeron/logbuffer/term_scanner.h"
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
  std::cout << "Sender::add_publication called: session=" << session_id
            << ", stream=" << stream_id << ", channel=\"" << channel << "\""
            << std::endl;

  std::lock_guard<std::mutex> lock(endpoints_mutex_);

  std::int64_t key = (static_cast<std::int64_t>(session_id) << 32) |
                     static_cast<std::uint32_t>(stream_id);

  auto endpoint = std::make_unique<SendEndpoint>();
  endpoint->session_id = session_id;
  endpoint->stream_id = stream_id;
  endpoint->channel = channel;

  bool parse_success = parse_channel_uri(channel, *endpoint);
  std::cout << "parse_channel_uri result: "
            << (parse_success ? "SUCCESS" : "FAILED") << std::endl;

  bool socket_success = create_socket(*endpoint);
  std::cout << "create_socket result: "
            << (socket_success ? "SUCCESS" : "FAILED") << std::endl;

  if (parse_success && socket_success) {
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

void Sender::set_log_buffer_manager(std::shared_ptr<LogBufferManager> manager) {
  log_buffer_manager_ = manager;
}

void Sender::process_publications() {
  if (!log_buffer_manager_) {
    return;
  }

  auto publications = log_buffer_manager_->get_all_publications();

  for (const auto &publication : publications) {
    // Find corresponding endpoint
    std::int64_t key =
        (static_cast<std::int64_t>(publication->session_id) << 32) |
        static_cast<std::uint32_t>(publication->stream_id);

    std::lock_guard<std::mutex> lock(endpoints_mutex_);
    auto it = endpoints_.find(key);
    if (it != endpoints_.end()) {
      send_publication_data(*it->second, publication);
    }
  }
}

void Sender::send_publication_data(
    SendEndpoint &endpoint,
    std::shared_ptr<LogBufferManager::PublicationInfo> publication) {

  // For now, keep the old implementation as a fallback
  // TODO: Replace with proper NetworkPublication usage

  if (!publication->log_buffers) {
    return;
  }

  // Get current sender position
  std::int64_t senderPosition = endpoint.position;

  // Read tail position from log buffer metadata (like Java NetworkPublication)
  std::uint8_t *metadata = publication->log_buffers->logMetaDataBuffer();
  const std::int32_t tailCounterOffset =
      logbuffer::LogBufferDescriptor::TERM_TAIL_COUNTERS_OFFSET;
  std::atomic<std::int64_t> *tailCounter =
      reinterpret_cast<std::atomic<std::int64_t> *>(metadata +
                                                    tailCounterOffset);
  std::int64_t producerPosition = tailCounter->load();

  if (senderPosition >= producerPosition) {
    return; // No new data to send
  }

  // Calculate term offset and active partition (simplified - use partition 0)
  std::int32_t termLength = publication->log_buffers->termLength();
  std::int32_t termOffset =
      static_cast<std::int32_t>(senderPosition & (termLength - 1));
  std::int32_t activeIndex = 0; // Simplified

  // Get term buffer
  std::uint8_t *termBuffer = publication->log_buffers->buffer(activeIndex);

  // Scan for available data (like Java NetworkPublication.sendData())
  std::int32_t scanLimit =
      std::min(static_cast<std::int32_t>(producerPosition - senderPosition),
               static_cast<std::int32_t>(MAX_UDP_PAYLOAD_LENGTH));

  std::int64_t scanOutcome = logbuffer::TermScanner::scanForAvailability(
      termBuffer, termOffset, scanLimit, termLength);

  std::int32_t available = logbuffer::TermScanner::available(scanOutcome);

  if (available > 0) {
    // Send data directly from term buffer (like Java)
    send_data_frame(endpoint, termBuffer + termOffset, available);

    // Update sender position
    std::int32_t padding = logbuffer::TermScanner::padding(scanOutcome);
    endpoint.position = senderPosition + available + padding;
  }

  // Send setup frame periodically for new connections
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
  std::cout << "DEBUG: send_data_frame called with length=" << length
            << std::endl;
  std::cout.flush();

  if (length > MAX_UDP_PAYLOAD_LENGTH) {
    std::cout << "DEBUG: Sending fragmented message" << std::endl;
    std::cout.flush();
    send_fragmented_message(endpoint, data, length);
    return;
  }

  std::cout << "DEBUG: Creating data header" << std::endl;
  std::cout.flush();

  // Create data header
  protocol::DataHeader header;
  header.init();
  header.session_id = endpoint.session_id;
  header.stream_id = endpoint.stream_id;
  header.term_id = endpoint.term_id;
  header.term_offset = endpoint.term_offset;
  header.set_frame_length(protocol::DataHeader::HEADER_LENGTH + length);
  header.set_unfragmented();

  std::cout << "DEBUG: Creating packet buffer" << std::endl;
  std::cout.flush();

  // Send header + data
  std::vector<std::uint8_t> packet(protocol::DataHeader::HEADER_LENGTH +
                                   length);
  std::memcpy(packet.data(), &header, protocol::DataHeader::HEADER_LENGTH);
  std::memcpy(packet.data() + protocol::DataHeader::HEADER_LENGTH, data,
              length);

  std::cout << "DEBUG: About to call sendto, socket_fd=" << endpoint.socket_fd
            << std::endl;
  std::cout.flush();

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

  std::cout << "DEBUG: sendto returned: " << result << std::endl;
  std::cout.flush();

  if (result < 0) {
    std::cerr << "Failed to send data frame" << std::endl;
  } else {
    std::cout << "Sender: Sent " << result << " bytes to " << endpoint.channel
              << std::endl;
    endpoint.term_offset += length;
    endpoint.position += length;
  }
}

void Sender::send_setup_frame(SendEndpoint &endpoint) {
  protocol::SetupHeader setup_header;
  setup_header.init();
  setup_header.session_id = endpoint.session_id;
  setup_header.stream_id = endpoint.stream_id;
  setup_header.term_length = endpoint.term_length;
  setup_header.mtu_length = endpoint.mtu_length;
  setup_header.initial_term_id = endpoint.term_id;
  setup_header.active_term_id = endpoint.term_id;
  setup_header.term_offset = endpoint.term_offset;
  setup_header.set_frame_length(sizeof(setup_header));

#ifdef _WIN32
  int result =
      sendto(endpoint.socket_fd, reinterpret_cast<const char *>(&setup_header),
             sizeof(setup_header), 0,
             reinterpret_cast<const sockaddr *>(&endpoint.destination_addr),
             sizeof(endpoint.destination_addr));
#else
  ssize_t result =
      sendto(endpoint.socket_fd, &setup_header, sizeof(setup_header), 0,
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
  // Simple URI parsing for "aeron:udp?endpoint=host:port|control=host:port"
  if (channel.find("aeron:udp?endpoint=") != 0) {
    std::cerr << "Unsupported channel URI: " << channel << std::endl;
    return false;
  }

  std::string endpoint_str = channel.substr(19); // Skip "aeron:udp?endpoint="

  // Handle optional control part: "|control=host:port"
  size_t pipe_pos = endpoint_str.find('|');
  if (pipe_pos != std::string::npos) {
    endpoint_str = endpoint_str.substr(0, pipe_pos);
  }

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
