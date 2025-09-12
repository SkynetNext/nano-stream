#include "aeron/driver/media/send_channel_endpoint.h"
#include <algorithm>
#include <iostream>

#ifdef _WIN32
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#endif

namespace aeron {
namespace driver {
namespace media {

SendChannelEndpoint::SendChannelEndpoint(const std::string &channel,
                                         std::int32_t port)
    : channel_(channel), port_(port) {

#ifdef _WIN32
  // Initialize Winsock
  WSADATA wsaData;
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    std::cerr << "WSAStartup failed" << std::endl;
    return;
  }
#endif

  // Create UDP socket
  socket_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

#ifdef _WIN32
  if (socket_ == INVALID_SOCKET) {
    std::cerr << "Failed to create socket" << std::endl;
    WSACleanup();
    return;
  }
#else
  if (socket_ < 0) {
    std::cerr << "Failed to create socket" << std::endl;
    return;
  }
#endif

  // Set socket options
  int broadcast = 1;
  setsockopt(socket_, SOL_SOCKET, SO_BROADCAST,
             reinterpret_cast<char *>(&broadcast), sizeof(broadcast));
}

SendChannelEndpoint::~SendChannelEndpoint() { close(); }

void SendChannelEndpoint::registerForSend(
    std::shared_ptr<NetworkPublication> publication) {
  publications_.push_back(publication);
}

void SendChannelEndpoint::unregisterForSend(
    std::shared_ptr<NetworkPublication> publication) {
  auto it = std::find(publications_.begin(), publications_.end(), publication);
  if (it != publications_.end()) {
    publications_.erase(it);
  }
}

std::int32_t SendChannelEndpoint::send(const std::uint8_t *buffer,
                                       std::int32_t length) {
#ifdef _WIN32
  if (closed_ || socket_ == INVALID_SOCKET) {
#else
  if (closed_ || socket_ < 0) {
#endif
    return -1;
  }

  // For now, send to localhost:port_ (simplified)
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<std::uint16_t>(port_));
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");

#ifdef _WIN32
  std::int32_t sent =
      sendto(socket_, reinterpret_cast<const char *>(buffer), length, 0,
             reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
#else
  std::int32_t sent =
      sendto(socket_, buffer, length, 0,
             reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
#endif

  if (sent < 0) {
#ifdef _WIN32
    std::cerr << "sendto failed: " << WSAGetLastError() << std::endl;
#else
    std::cerr << "sendto failed" << std::endl;
#endif
  }

  return sent;
}

void SendChannelEndpoint::close() {
  if (!closed_) {
    closed_ = true;

#ifdef _WIN32
    if (socket_ != INVALID_SOCKET) {
      closesocket(socket_);
      socket_ = INVALID_SOCKET;
    }
    WSACleanup();
#else
    if (socket_ >= 0) {
      ::close(socket_);
      socket_ = -1;
    }
#endif
  }
}

} // namespace media
} // namespace driver
} // namespace aeron
