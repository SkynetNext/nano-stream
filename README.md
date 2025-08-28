# Nano-Stream

A high-performance, low-latency C++ library for inter-process and inter-thread communication, inspired by **LMAX Disruptor** and **Aeron**.

## Features

- 🚀 **Lock-free Ring Buffer**: Ultra-fast single-producer, multiple-consumer ring buffer
- ⚡ **Nanosecond Latency**: Optimized for high-frequency trading and real-time systems
- 🧵 **Cache-Line Optimized**: Minimizes false sharing with proper memory alignment
- 🔧 **Modern C++20**: Uses latest C++ features for optimal performance
- 📦 **Header-Only**: Easy integration with no linking required
- 📊 **Comprehensive Benchmarks**: Extensive performance testing and comparison
- 🧪 **Thoroughly Tested**: Full unit test coverage with Google Test

## Quick Start

### Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### Basic Usage

```cpp
#include "nano_stream.h"  // Single header include
using namespace nano_stream;

// Create a ring buffer for your events
struct TradeEvent {
    int64_t id;
    double price;
    int64_t quantity;
};

RingBuffer<TradeEvent> ring_buffer(1024, []() { return TradeEvent(); });

// Producer
int64_t sequence = ring_buffer.next();
TradeEvent& event = ring_buffer.get(sequence);
event.id = 123;
event.price = 100.50;
event.quantity = 1000;
ring_buffer.publish(sequence);

// Consumer
if (ring_buffer.is_available(sequence)) {
    const TradeEvent& event = ring_buffer.get(sequence);
    // Process event...
}
```

## Performance

Benchmarked on Intel 4-core 3.6GHz CPU with Clang 19.1.1:

### 🚀 **Core Operations**
- **Sequence Get**: 0.51 ns (2B ops/sec)
- **Sequence Set**: 0.26 ns (3.9B ops/sec)
- **Ring Buffer Single Producer**: 3.7-4.2 ns (241-278M ops/sec)
- **Ring Buffer Sequential Access**: 0.74 ns (1.4B ops/sec)

### 🏆 **Producer-Consumer Performance**
- **Nano-Stream**: 17-21M items/sec
- **std::queue + mutex**: 15-17M items/sec
- **Performance gain**: **+36-46%** over std::queue

### 📊 **Batch Processing**
- **8-item batch**: 566M items/sec
- **64-item batch**: 665M items/sec
- **Optimal for high-throughput scenarios**

### ⚡ **Key Achievements**
- **Zero memory allocation** during operation
- **Cache-friendly access patterns** for optimal CPU utilization
- **Nanosecond-level latency** for time-critical applications
- **Linear scaling** with batch sizes

## Architecture

### Core Components

1. **Sequence**: Atomic sequence number with cache-line padding
2. **RingBuffer**: Lock-free circular buffer with pre-allocated entries
3. **EventFactory**: Interface for creating events

### Memory Layout

```
Cache Line 1: [Sequence Value + Padding]
Cache Line 2: [Ring Buffer Data + Padding]  
Cache Line 3: [More Ring Buffer Data...]
```

## Comparison with Alternatives

| Feature | Nano-Stream | std::queue + mutex | LMAX Disruptor (Java) |
|---------|-------------|-------------------|----------------------|
| **Latency** | **0.26-4.2ns** | ~100ns | ~3ns |
| **Throughput** | **21M items/sec** | 15M items/sec | 25M+ ops/sec |
| **Single-threaded** | **325M ops/sec** | 189M ops/sec | N/A |
| **Batch processing** | **665M items/sec** | Limited | Excellent |
| **Memory Allocation** | Zero | Per operation | Zero |
| **Lock-free** | ✅ | ❌ | ✅ |
| **Language** | C++20 | C++ | Java |

## Running Tests

```bash
# Unit tests
./build/tests/nano_stream_tests

# Benchmarks
./build/benchmarks/nano_stream_benchmarks

# Example
./build/examples/basic_example
```

## Requirements

- C++20 compatible compiler (GCC 10+, Clang 11+, MSVC 2019+)
- CMake 3.20+
- Google Test (automatically downloaded)
- Google Benchmark (automatically downloaded)

## License

Licensed under the Apache License 2.0. See [LICENSE](LICENSE) for details.

## Inspiration

This project is inspired by:
- [LMAX Disruptor](https://github.com/LMAX-Exchange/disruptor) - Ultra-fast inter-thread messaging
- [Aeron](https://github.com/real-logic/aeron) - High-performance messaging transport

## Contributing

Contributions are welcome! Please read our [Contributing Guide](CONTRIBUTING.md) for details.
