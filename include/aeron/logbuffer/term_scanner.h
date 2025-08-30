#pragma once

#include <cstdint>

namespace aeron {
namespace logbuffer {

/**
 * Scans a term buffer for availability of new message fragments.
 * Based on Java TermScanner.
 */
class TermScanner {
public:
  /**
   * Scan result packed into a long.
   * Upper 32 bits: padding, Lower 32 bits: available bytes
   */
  static std::int64_t scanForAvailability(const std::uint8_t *termBuffer,
                                          std::int32_t offset,
                                          std::int32_t maxLength,
                                          std::int32_t termLength);

  /**
   * Extract available bytes from scan result.
   */
  static std::int32_t available(std::int64_t scanResult) {
    return static_cast<std::int32_t>(scanResult);
  }

  /**
   * Extract padding from scan result.
   */
  static std::int32_t padding(std::int64_t scanResult) {
    return static_cast<std::int32_t>(scanResult >> 32);
  }

private:
  static std::int64_t pack(std::int32_t padding, std::int32_t available) {
    return (static_cast<std::int64_t>(padding) << 32) |
           static_cast<std::uint32_t>(available);
  }
};

} // namespace logbuffer
} // namespace aeron
