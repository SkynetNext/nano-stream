#pragma once

/**
 * Disruptor: High-Performance Lock-Free Communication Library
 *
 * A C++ implementation inspired by LMAX Disruptor for high-performance,
 * low-latency inter-thread communication.
 */

// Main library headers - exported for convenience API
#include "consumer.h"         // IWYU pragma: export
#include "event_handler.h"    // IWYU pragma: export
#include "event_translator.h" // IWYU pragma: export
#include "ring_buffer.h"      // IWYU pragma: export
#include "sequence.h"         // IWYU pragma: export

#include <string>

namespace disruptor {

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

} // namespace disruptor
