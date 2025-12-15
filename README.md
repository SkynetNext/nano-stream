# Nano-Stream

A high-performance, low-latency C++ library for inter-process and inter-thread communication, inspired by **LMAX Disruptor** and **Aeron**.

## ✨ Features

- 🚀 **Lock-free Ring Buffer**: Ultra-fast single-producer, multiple-consumer ring buffer
- ⚡ **Nanosecond Latency**: 0.26-4.2ns operations for time-critical applications
- 🧵 **Cache-Line Optimized**: Minimizes false sharing with proper memory alignment
- 📦 **Header-Only**: Easy integration with no linking required
- 🔧 **Modern C++20**: Uses latest C++ features for optimal performance

## 🚀 Quick Start

```cpp
#include "nano_stream.h"
using namespace nano_stream;

// Create ring buffer
RingBuffer<TradeEvent> ring_buffer(1024, []() { return TradeEvent(); });

// Producer
int64_t sequence = ring_buffer.next();
TradeEvent& event = ring_buffer.get(sequence);
event.id = 123;
ring_buffer.publish(sequence);

// Consumer  
if (ring_buffer.is_available(sequence)) {
    const TradeEvent& event = ring_buffer.get(sequence);
    // Process event...
}
```

## 📊 Performance

**36-46% faster** than `std::queue + mutex`:
- **Core Operations**: 0.26-4.2ns (up to 3.9B ops/sec)
- **Producer-Consumer**: 17-21M items/sec
- **Batch Processing**: 665M items/sec (64-item batches)

> 📈 [**Full Benchmark Results**](docs/BENCHMARK_RESULTS.md) | 🏗️ [**Build Guide**](docs/BUILD_GUIDE.md)

## 🛠️ Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## 📚 Documentation

- 🏗️ [**Architecture**](docs/ARCHITECTURE.md) - System design and components
- 📊 [**Benchmark Results**](docs/BENCHMARK_RESULTS.md) - Performance analysis
- 🛠️ [**Build Guide**](docs/BUILD_GUIDE.md) - Build instructions  
- 📋 [**Project Status**](docs/PROJECT_STATUS.md) - Development roadmap

## 🧪 Testing

```bash
./build/tests/nano_stream_tests      # Unit tests
./build/benchmarks/nano_stream_benchmarks  # Performance benchmarks
./build/examples/basic_example       # Usage example
```

## 🎯 Use Cases

Perfect for:
- **High-Frequency Trading**: Nanosecond-level latency
- **Real-time Systems**: Zero-allocation, predictable performance  
- **Game Engines**: Frame-critical event processing
- **Streaming Applications**: High-throughput data flows

## 📄 License

Licensed under the Apache License 2.0. See [LICENSE](LICENSE) for details.

## 🙏 Inspiration

- [LMAX Disruptor](https://github.com/LMAX-Exchange/disruptor) - Ultra-fast inter-thread messaging
- [Aeron](https://github.com/real-logic/aeron) - High-performance messaging transport
