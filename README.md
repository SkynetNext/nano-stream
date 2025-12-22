# Disruptor-CPP

A high-performance, low-latency C++ library for inter-thread communication, featuring a **1:1 C++ port of LMAX Disruptor** (API + tests), plus aligned C++ benchmarks for fair comparison against Java JMH.

## Features

- **Disruptor (C++ port)**: `include/disruptor/**` with namespaces `disruptor`, `disruptor::dsl`, `disruptor::util`
- **Tests**: Java tests ported to GoogleTest under `tests/disruptor/**`
- **Benchmarks**: JMH-aligned C++ benchmarks under `benchmarks/jmh/**` (Google Benchmark), comparable to Java JMH results

## Quick Start (build)

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

## Benchmarking (aligned with CI)

Run all C++ JMH-ported benchmarks and output JSON:

```powershell
cd F:\disruptor-cpp\build
.\benchmarks\nano_stream_benchmarks.exe --benchmark_filter='^JMH_' --benchmark_min_warmup_time=10 --benchmark_min_time=5s --benchmark_repetitions=3 --benchmark_report_aggregates_only=true --benchmark_out=..\benchmark_cpp.json --benchmark_out_format=json
```

Run Java JMH (Git Bash, using the JMH jar) and output JSON:

```bash
cd /f/disruptor-cpp/reference/disruptor
./gradlew jmhJar --no-daemon
java -jar build/libs/*-jmh.jar -rf json -rff ../../benchmark_java.json -foe true -v NORMAL -f 1 -wi 2 -w 5s -i 3 -r 5s ".*(SingleProducerSingleConsumer|MultiProducerSingleConsumer|BlockingQueueBenchmark).*"
```

Then generate the comparison report:

```bash
bash scripts/compare_benchmarks.sh > comparison_report.md
```

The latest CI-generated comparison is written to `comparison_report.md`.

## Documentation

- **Benchmark Results**: `docs/BENCHMARK_RESULTS.md`
- **Build Guide**: `docs/BUILD_GUIDE.md`

## Testing

```bash
./build/tests/disruptor_cpp_tests      # Unit tests
./build/benchmarks/disruptor_cpp_benchmarks  # Performance benchmarks
./build/examples/basic_example       # Usage example
```

## 📄 License

Licensed under the Apache License 2.0. See [LICENSE](LICENSE) for details.

## Inspiration

- [LMAX Disruptor](https://github.com/LMAX-Exchange/disruptor) - Ultra-fast inter-thread messaging
- [Aeron](https://github.com/real-logic/aeron) - High-performance messaging transport
