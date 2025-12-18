# Benchmark Results (JMH-aligned Disruptor comparison)

## Test environment 

| Property | Value |
|----------|-------|
| Host | runnervmh13bl |
| Date (UTC) | 2025-12-18T08:21:46+00:00 |
| CPU Cores | 4 |
| CPU Frequency | 3244 MHz |
| CPU Scaling | Disabled |
| ASLR | Enabled |

### Cache information

| Cache | Details |
|------|---------|
| L1 Data | 32768 bytes (shared by 2 cores) |
| L1 Instruction | 32768 bytes (shared by 2 cores) |
| L2 Unified | 524288 bytes (shared by 2 cores) |
| L3 Unified | 33554432 bytes (shared by 4 cores) |

## How to run (same parameters as CI)

### C++ (Google Benchmark)

```powershell
cd F:\nano-stream\build
.\benchmarks\nano_stream_benchmarks.exe --benchmark_filter='^JMH_' --benchmark_min_warmup_time=10 --benchmark_min_time=5s --benchmark_repetitions=3 --benchmark_report_aggregates_only=true --benchmark_out=..\benchmark_cpp.json --benchmark_out_format=json
```

### Java (JMH, via jmhJar)

```bash
cd /f/nano-stream/reference/disruptor
./gradlew jmhJar --no-daemon
java -jar build/libs/*-jmh.jar -rf json -rff ../../benchmark_java.json -foe true -v NORMAL -f 1 -wi 2 -w 5s -i 3 -r 5s ".*(SingleProducerSingleConsumer|MultiProducerSingleConsumer|BlockingQueueBenchmark).*"
```

### Generate report

```bash
bash scripts/compare_benchmarks.sh > comparison_report.md
```

## Results Summary

### Key Performance Metrics

| Test | C++ Disruptor | Java Disruptor | C++ tbus | C++/Java Ratio |
|------|---------------|----------------|----------|----------------|
| **SPSC** | 462.7 Mops/sec | 130.3 Mops/sec | 47.9 Mops/sec | 3.55x |
| **MP1C** | 553.0 Mops/sec | 36.4 Mops/sec | N/A | 15.2x |
| **MP1C Batch** | 1650.0 Mops/sec | 204.6 Mops/sec | N/A | 8.06x |
| **BlockingQueue** | 11.3 Mops/sec | 9.4 Mops/sec | N/A | 1.20x |

**Notes:**
- **Units**: All results in **Mops/sec** (million operations per second)
- **SPSC**: Single Producer Single Consumer
- **MP1C**: Multi Producer Single Consumer (4 threads)
- **tbus**: TSF4G tbus-style minimal SPSC implementation (C++ only)
- **N/A**: Test not available for this implementation

### Data Sources

**C++ (from `benchmark_cpp.json`):**
- SPSC: `JMH_SingleProducerSingleConsumer_producing_mean` → 2.161 ns/op → 462.7 Mops/sec
- MP1C: `JMH_MultiProducerSingleConsumer_producing/threads:4_mean` → 7.233 ns/op/thread → 553.0 Mops/sec (4 threads)
- MP1C Batch: `JMH_MultiProducerSingleConsumer_producingBatch/threads:4_mean` → 1650.0 Mops/sec (from items_per_second)
- BlockingQueue: `JMH_BlockingQueueBenchmark_producing_mean` → 88.56 ns/op → 11.3 Mops/sec
- tbus: `JMH_TBusSingleProducerSingleConsumer_producing_mean` → 20.87 ns/op → 47.9 Mops/sec

**Java (from `benchmark_java.json`):**
- SPSC: `SingleProducerSingleConsumer.producing` (avgt) → 7.674 ns/op → 130.3 Mops/sec
- MP1C: `MultiProducerSingleConsumer.producing` (thrpt) → 36407.6 ops/ms → 36.4 Mops/sec
- MP1C Batch: `MultiProducerSingleConsumer.producingBatch` (thrpt) → 204558.0 ops/ms → 204.6 Mops/sec
- BlockingQueue: `BlockingQueueBenchmark.producing` (avgt) → 105.88 ns/op → 9.4 Mops/sec

## Caveats for interpreting cross-language numbers

- **Different harnesses**: Google Benchmark vs JMH (forked JVM, different warmup model). We align parameters, but they are still not identical runtimes.
- **OS/CPU differences**: Always compare results collected on the same machine if you want to attribute deltas to implementation differences.
- **Thread scheduling**: MP benchmarks are sensitive to core pinning, background load, and timer resolution.

## Analysis

- **C++ Disruptor significantly outperforms Java** across all test scenarios (3.55x to 15.2x faster).
- **C++ tbus** (minimal SPSC) is slower than both C++ and Java Disruptor, suggesting the Disruptor implementation has optimizations beyond basic ring buffer mechanics.
- **BlockingQueue baseline**: C++ and Java are close (1.20x ratio), validating the baseline implementation.
