#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

namespace aeron {
namespace driver {
namespace media {

/**
 * SendChannelEndpoint manages UDP socket for sending data.
 * This is the C++ equivalent of Java's SendChannelEndpoint.
 */
class SendChannelEndpoint {
public:
  SendChannelEndpoint(const std::string &channel, std::int32_t port);
  ~SendChannelEndpoint();

  /**
   * Register a publication for sending.
   */
  void registerForSend(std::shared_ptr<class NetworkPublication> publication);

  /**
   * Unregister a publication from sending.
   */
  void unregisterForSend(std::shared_ptr<class NetworkPublication> publication);

  /**
   * Send data to the network.
   */
  std::int32_t send(const std::uint8_t *buffer, std::int32_t length);

  /**
   * Get the channel string.
   */
  const std::string &channel() const { return channel_; }

  /**
   * Get the port.
   */
  std::int32_t port() const { return port_; }

  /**
   * Check if the endpoint is closed.
   */
  bool isClosed() const { return closed_; }

  /**
   * Close the endpoint.
   */
  void close();

private:
  std::string channel_;
  std::int32_t port_;
  bool closed_{false};

  // Socket handle (platform specific)
#ifdef _WIN32
  SOCKET socket_{INVALID_SOCKET};
#else
  int socket_{-1};
#endif

  std::vector<std::shared_ptr<class NetworkPublication>> publications_;
};

} // namespace media
} // namespace driver
} // namespace aeron
