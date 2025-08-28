#pragma once

/**
 * Nano-Stream: High-Performance Lock-Free Communication Library
 *
 * A C++ library inspired by LMAX Disruptor and Aeron for high-performance,
 * low-latency inter-process and inter-thread communication.
 *
 * This is the main header file that includes all Nano-Stream functionality.
 * Simply include this file to use the library.
 *
 * @author  Nano-Stream Team
 * @version 1.0.0
 * @date    2025
 *
 * Example usage:
 * @code
 * #include "nano_stream.h"
 *
 * using namespace nano_stream;
 *
 * // Create a ring buffer for your events
 * struct Event { int data; };
 * RingBuffer<Event> buffer(1024, []() { return Event(); });
 *
 * // Producer
 * int64_t seq = buffer.next();
 * Event& event = buffer.get(seq);
 * event.data = 42;
 * buffer.publish(seq);
 *
 * // Consumer
 * if (buffer.is_available(seq)) {
 *     const Event& event = buffer.get(seq);
 *     // Process event...
 * }
 * @endcode
 */

// Core components
#include "nano_stream/ring_buffer.h" // IWYU pragma: export
#include "nano_stream/sequence.h"    // IWYU pragma: export

// Main namespace and version info
#include "nano_stream/nano_stream.h" // IWYU pragma: export

/**
 * @brief Nano-Stream namespace containing all library functionality
 *
 * This namespace contains:
 * - Sequence: Cache-line aligned atomic sequence numbers
 * - RingBuffer: Lock-free ring buffer for high-performance communication
 * - Version: Library version information
 */
namespace nano_stream {

// Library documentation and feature overview
/**
 * @defgroup core Core Components
 * @brief Core lock-free data structures
 *
 * The core components provide the fundamental building blocks for
 * high-performance, low-latency communication:
 *
 * - @ref Sequence: Atomic sequence numbers with cache-line alignment
 * - @ref RingBuffer: Lock-free ring buffer with pre-allocated entries
 */

/**
 * @defgroup performance Performance Features
 * @brief Performance optimization features
 *
 * Nano-Stream is designed for maximum performance:
 *
 * - **Lock-free**: All operations are wait-free or lock-free
 * - **Cache-optimized**: Memory layout minimizes cache misses
 * - **Zero-allocation**: No memory allocation during operation
 * - **NUMA-aware**: Can be configured for NUMA topology
 */

/**
 * @defgroup patterns Usage Patterns
 * @brief Common usage patterns and best practices
 *
 * Common patterns for using Nano-Stream:
 *
 * - Single Producer, Single Consumer (SPSC)
 * - Single Producer, Multiple Consumer (SPMC)
 * - Batch processing for higher throughput
 * - Event sourcing and CQRS patterns
 */

} // namespace nano_stream
