#pragma once

/**
 * Nano-Stream Aeron C++ Implementation - Header Only Library
 *
 * A high-performance messaging library based on the LMAX Disruptor pattern.
 * This implementation provides both in-process and inter-process communication.
 *
 * Usage:
 *   #include "aeron.h"
 *   // Use aeron_simple for in-process communication
 *   // Use aeron::ipc for inter-process communication
 */

// Import nano-stream foundation
#include "nano_stream/nano_stream.h"

// Simple Aeron (in-process communication)
#include "aeron/simple_aeron.h"

// Advanced IPC Aeron (currently placeholder for future shared memory
// implementation) Note: These require fixing for true inter-process
// communication #include "aeron/util/memory_mapped_file.h" #include
// "aeron/ipc/ipc_publication.h" #include "aeron/ipc/ipc_subscription.h"

namespace aeron {

/**
 * Version information for the Aeron C++ implementation.
 */
struct Version {
  static constexpr int MAJOR = 1;
  static constexpr int MINOR = 0;
  static constexpr int PATCH = 0;

  static std::string get_version_string() {
    return std::to_string(MAJOR) + "." + std::to_string(MINOR) + "." +
           std::to_string(PATCH);
  }

  static std::string get_full_version_string() {
    return "Nano-Stream Aeron C++ " + get_version_string();
  }
};

} // namespace aeron

// Convenience aliases for easy usage
namespace aeron_simple = aeron::simple;
using SimpleEvent = aeron::simple::SimpleEvent;
