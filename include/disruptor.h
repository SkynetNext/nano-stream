#pragma once

/**
 * Disruptor: High-Performance Lock-Free Ring Buffer Library
 *
 * A C++ implementation of the LMAX Disruptor pattern for
 * high-performance, low-latency inter-thread communication.
 *
 * This is the main header file that includes all Disruptor functionality.
 * Simply include this file to use the library.
 *
 * @author  Disruptor Team
 * @version 1.0.0
 * @date    2025
 *
 * Example usage:
 * @code
 * #include "disruptor.h"
 *
 * using namespace disruptor;
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
#include "disruptor/ring_buffer.h" // IWYU pragma: export
#include "disruptor/sequence.h"    // IWYU pragma: export

// SPMC support components
#include "disruptor/batch_event_processor.h" // IWYU pragma: export
#include "disruptor/disruptor_dsl.h"         // IWYU pragma: export
#include "disruptor/sequence_barrier.h"      // IWYU pragma: export

// Main namespace and version info
#include "disruptor/disruptor.h" // IWYU pragma: export

/**
 * @brief Disruptor namespace containing all library functionality
 *
 * This namespace contains:
 * - Sequence: Cache-line aligned atomic sequence numbers
 * - RingBuffer: Lock-free ring buffer for high-performance communication
 * - Version: Library version information
 */
namespace disruptor {

// Library documentation and feature overview
/**
 * @defgroup core Core Components
 * @brief Core lock-free data structures
 *
 * The core components provide the fundamental building blocks for
 * high-performance, low-latency inter-thread communication:
 *
 * - @ref Sequence: Atomic sequence numbers with cache-line alignment
 * - @ref RingBuffer: Lock-free ring buffer with pre-allocated entries
 */

/**
 * @defgroup performance Performance Features
 * @brief Performance optimization features
 *
 * Disruptor is designed for maximum performance:
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
 * Common patterns for using Disruptor:
 *
 * - Single Producer, Single Consumer (SPSC) - UniCast
 * - Single Producer, Multiple Consumer (SPMC) - Pipeline/Multicast
 * - Multiple Producer, Single Consumer (MPSC) - Sequence
 * - Batch processing for higher throughput
 * - Event sourcing and CQRS patterns
 */

} // namespace disruptor
