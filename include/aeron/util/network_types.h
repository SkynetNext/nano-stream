#pragma once

#include <cstdint>
#include <string>

namespace aeron {
namespace util {

/**
 * Simple network address structure to avoid platform dependencies.
 */
struct NetworkAddress {
  std::uint8_t addr[128]; // Large enough for any address type
  std::uint32_t family;
  std::uint16_t port;

  NetworkAddress() : family(0), port(0) {
    for (int i = 0; i < 128; ++i) {
      addr[i] = 0;
    }
  }
};

/**
 * Network utilities without platform-specific headers.
 */
namespace NetworkUtils {

/**
 * Parse an endpoint string like "localhost:8080" into address components.
 */
bool parse_endpoint(const std::string &endpoint, NetworkAddress &addr);

/**
 * Convert a network address to string representation.
 */
std::string address_to_string(const NetworkAddress &addr);

/**
 * Check if an address is multicast.
 */
bool is_multicast(const NetworkAddress &addr);

} // namespace NetworkUtils

} // namespace util
} // namespace aeron
