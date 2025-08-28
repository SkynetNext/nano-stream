#pragma once

/**
 * Nano-Stream: High-Performance Lock-Free Communication Library
 *
 * A C++ library inspired by LMAX Disruptor and Aeron for high-performance,
 * low-latency inter-process and inter-thread communication.
 */

// Main library headers - exported for convenience API
#include "ring_buffer.h" // IWYU pragma: export
#include "sequence.h"    // IWYU pragma: export

#include <string>

namespace nano_stream {

/**
 * Library version information
 */
struct Version {
  static constexpr int MAJOR = 1;
  static constexpr int MINOR = 0;
  static constexpr int PATCH = 0;

  static std::string get_version_string() {
    return std::to_string(MAJOR) + "." + std::to_string(MINOR) + "." +
           std::to_string(PATCH);
  }
};

} // namespace nano_stream
