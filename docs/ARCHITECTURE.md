# Disruptor-CPP Architecture

High-performance, low-latency C++ communication library inspired by LMAX Disruptor and Aeron.

## Design Philosophy

Disruptor-CPP combines the best ideas from:
- **LMAX Disruptor**: Lock-free ring buffer patterns for inter-thread communication
- **Aeron**: Zero-copy, high-performance messaging for inter-process communication

### Core Principles

- **Zero Allocation**: Pre-allocated buffers eliminate runtime memory allocation
- **Cache-Line Optimized**: Proper memory alignment prevents false sharing
- **Lock-Free Algorithms**: Atomic operations for maximum performance
- **Low Latency**: Optimized for time-critical applications (~2.2ns SPSC micro-benchmark)

## System Architecture

### Component Overview

```
┌─────────────────────────────────────────────────────────┐
│                    Disruptor-CPP                        │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌──────────────┐         ┌──────────────┐             │
│  │  Disruptor   │         │    Aeron     │             │
│  │   (Phase 2)  │         │  (Phase 3)   │             │
│  │              │         │              │             │
│  │ RingBuffer   │         │ Media Driver │             │
│  │ Sequence     │         │ Publication  │             │
│  │ Consumer     │         │ Subscription │             │
│  │ WaitStrategy │         │ Log Buffers  │             │
│  └──────────────┘         └──────────────┘             │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

## Disruptor Architecture (Phase 1-2)

### Ring Buffer

The ring buffer is the core data structure, providing:
- **Pre-allocated events**: Fixed-size buffer eliminates allocation overhead
- **Power-of-2 sizing**: Enables fast modulo operations using bitwise AND
- **Cache-line alignment**: Prevents false sharing between producer and consumers

```cpp
template<typename T>
class RingBuffer {
    Sequence cursor_;              // Producer position
    std::vector<T> entries_;        // Pre-allocated events
    std::vector<Sequence> gating_;  // Consumer positions
};
```

### Sequence

Cache-line aligned atomic sequence number for progress tracking:

```cpp
class alignas(std::hardware_destructive_interference_size) Sequence {
    std::atomic<int64_t> value_;
    // Padding to fill cache line
};
```

**Key Features:**
- Cache-line alignment (64 bytes) prevents false sharing
- Memory ordering optimizations (acquire/release semantics)
- Lock-free atomic operations

### Producer Types

#### Single Producer
- Direct assignment (no atomic operations)
- **End-to-end**: 127 Mops/sec (~7.9ns/op) - representative of real-world usage
- **Micro-benchmark**: 462.7 Mops/sec (~2.2ns/op) - theoretical maximum

#### Multi Producer
- Atomic CAS operations
- ~300M ops/sec throughput
- Thread-safe for concurrent producers

### Wait Strategies

Different strategies for handling backpressure:

- **BusySpinWaitStrategy**: Lowest latency, highest CPU usage
- **YieldingWaitStrategy**: Balanced (default)
- **BlockingWaitStrategy**: Lowest CPU usage, highest latency
- **SleepingWaitStrategy**: Exponential backoff

### Event Processing

```cpp
// Producer
int64_t sequence = ring_buffer.next();
Event& event = ring_buffer.get(sequence);
event.data = ...;
ring_buffer.publish(sequence);

// Consumer
if (ring_buffer.is_available(sequence)) {
    const Event& event = ring_buffer.get(sequence);
    // Process event...
}
```

## Aeron Architecture (Phase 3 - Planned)

### Media Driver

Independent process managing shared memory and network I/O:

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│   Client A   │    │   Client B   │    │   Client C   │
│ (Publisher)  │    │(Subscriber)  │    │(Subscriber)  │
└──────┬───────┘    └──────┬───────┘    └──────┬───────┘
       │                   │                   │
       └───────────────────┼───────────────────┘
                           │
                  ┌─────────┴─────────┐
                  │   Media Driver    │
                  │  (Shared Memory)  │
                  └───────────────────┘
```

### Log Buffers

Three-term rotating buffer for zero-copy messaging:

```
┌─────────────────────────────────────┐
│         Log Buffer                  │
├─────────────────────────────────────┤
│  Term 0  │  Term 1  │  Term 2  │ M │
│  (Active)│  (Next)  │  (Clean)  │ e │
│          │          │           │ t │
└─────────────────────────────────────┘
```

### Publication/Subscription

- **Publication**: Write path for publishers
- **Subscription**: Read path for subscribers
- **Image**: Per-subscriber view of a publication

## Memory Layout

### Cache-Line Optimization

```
┌─────────────────────────────────────────┐
│  Cache Line 1 (64 bytes)                │
│  Sequence::value_ + padding             │
├─────────────────────────────────────────┤
│  Cache Line 2 (64 bytes)                │
│  RingBuffer fields + padding            │
├─────────────────────────────────────────┤
│  Cache Line 3+                          │
│  Pre-allocated Event Array              │
└─────────────────────────────────────────┘
```

### False Sharing Prevention

Each `Sequence` instance is aligned to cache-line boundaries:
- Prevents false sharing between producer and consumers
- Reduces cache coherency traffic
- Improves multi-core performance

## Performance Characteristics

*Note: End-to-end tests (OneToOneSequencedThroughputTest) are more representative of real-world performance than micro-benchmarks, as they include full system overhead (thread scheduling, wait strategies, batch processing, synchronization).*

### End-to-End Performance (Production-like)

- **SPSC End-to-End**: 
  - C++: 127 Mops/sec (~7.9ns/op)
  - Java: 111 Mops/sec (~9.0ns/op)
  - **C++ is 1.14x faster** - minimal difference in practical scenarios

### Micro-Benchmark Performance

- **SPSC micro-benchmark**: 2.161 ns/op (C++), 7.674 ns/op (Java) → 462.7 Mops/sec (C++), 130.3 Mops/sec (Java)
- **MP1C (4 threads)**: 40.7 Mops/sec (C++), 36.2 Mops/sec (Java)
- **MP1C Batch (4 threads)**: 208.9 Mops/sec (C++), 208.7 Mops/sec (Java)
- **BlockingQueue baseline**: 88.56 ns/op (C++), 105.88 ns/op (Java) → 11.3 Mops/sec (C++), 9.4 Mops/sec (Java)

### Scalability

- Linear scaling with batch size
- Minimal overhead in multi-threaded scenarios
- Zero allocations during operation

## Design Patterns

### Single Writer Principle

Only one thread writes to each sequence, eliminating write contention.

### Event Factory Pattern

```cpp
RingBuffer<Event> buffer(size, []() { return Event(); });
```

Pre-allocates all events at construction time.

### Zero-Copy Design

Events are reused, only their content is updated. No copying during operation.

## Use Cases

### High-Frequency Trading
- Sub-nanosecond latency requirements
- Predictable performance
- Zero GC pauses

### Real-Time Systems
- Consistent latency
- High throughput
- Memory-efficient

### Game Engines
- Frame-critical event processing
- Low-latency input handling
- Efficient event distribution

## Comparison with Alternatives

*Note: Performance metrics are based on end-to-end tests (OneToOneSequencedThroughputTest), which better reflect real-world usage than micro-benchmarks.*

| Feature | Disruptor-CPP | std::queue + mutex | LMAX Disruptor |
|---------|-------------|-------------------|----------------|
| Language | C++ | C++ | Java |
| Latency (SPSC End-to-End) | ~7.9ns | ~88ns | ~9.0ns |
| Throughput (SPSC End-to-End) | 127 Mops/sec | 11.3 Mops/sec | 111 Mops/sec |
| Allocation | Zero | Per-op | Zero |
| Lock-free | ✅ | ❌ | ✅ |
| Cache Optimized | ✅ | ❌ | ✅ |

## Future Enhancements

- **Shared Memory IPC**: Aeron-style inter-process communication
- **Network Transport**: UDP unicast and multicast
- **NUMA Awareness**: Optimize for multi-socket systems
- **Huge Pages**: Reduce TLB misses
- **SIMD Optimizations**: Vectorized operations

