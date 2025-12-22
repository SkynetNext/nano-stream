# Disruptor-CPP Documentation

High-performance, low-latency C++ communication library inspired by LMAX Disruptor and Aeron.

## Core Documentation

- **[Architecture](ARCHITECTURE.md)** - System design, components, and design principles
- **[Build Guide](BUILD_GUIDE.md)** - Building and integration instructions
- **[Benchmark Results](BENCHMARK_RESULTS.md)** - Performance metrics and analysis

## Quick Links

- [Project Status](../README.md) - Overview and features
- [Source Code](../include/) - Header-only implementation

## Design Philosophy

Disruptor-CPP combines the best ideas from:
- **LMAX Disruptor**: Lock-free ring buffer patterns
- **Aeron**: Zero-copy, high-performance messaging

Key principles:
- Zero allocation during operation
- Cache-line optimized memory layout
- Lock-free algorithms
- Low latency: ~7.9ns/op (SPSC end-to-end, C++), ~9.0ns/op (Java)
- High throughput: 127 Mops/sec (SPSC end-to-end, C++), 111 Mops/sec (Java)
