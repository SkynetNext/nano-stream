# Benchmark Results (JMH-aligned Disruptor comparison)

This repository contains a **1:1 C++ port of LMAX Disruptor** plus **aligned benchmarks** so we can compare:

- **C++ (Google Benchmark)**: `benchmarks/jmh/**` (benchmarks are named `JMH_*`)
- **Java (JMH)**: `reference/disruptor/src/jmh/java/**`

The numbers below are taken from:

- `benchmark_cpp.json` (Google Benchmark JSON, aggregates)
- `benchmark_java.json` (JMH JSON)

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

## Results summary (from the provided JSON)

> Note on units:
> - C++ aggregate `real_time` is **ns/op** (for these benchmarks). Convert to ops/s via \( \text{ops/s} = 10^9 / \text{ns/op} \).
> - For multi-threaded C++ benchmarks, Google Benchmark reports **time per operation per thread**; total throughput is roughly \( \text{threads} \times 10^9 / \text{ns/op} \).
> - Java `thrpt` results in this repo are in **ops/ms**; convert to ops/s by multiplying by 1000.

### Single Producer / Single Consumer (SPSC)

Using the latest report (Generated: 2025-12-17 10:32 UTC):

- **Java Disruptor** (`SingleProducerSingleConsumer.producing`, avgt): **~6.92 ns/op** → **~1.44e+08 ops/s**
- **C++ Disruptor port** (`JMH_SingleProducerSingleConsumer_producing_mean`): **~19.40 ns/op** → **~5.16e+07 ops/s**
- **C++ tbus-style SPSC baseline** (`JMH_TBusSingleProducerSingleConsumer_producing_mean`): **~15.51 ns/op** → **~6.45e+07 ops/s**

Interpretation:
- On this data set, **C++ SPSC is ~0.36x Java** (ops/s), so there is **significant optimization headroom** in the C++ port for this scenario.
- The included **tbus-like SPSC** is faster than the C++ Disruptor SPSC, suggesting remaining overhead in the C++ port path (or scheduling/affinity effects).

### Multi Producer / Single Consumer (MP1C)

- **Java Disruptor** (`MultiProducerSingleConsumer.producing`, thrpt): **~3.66e+07 ops/s** (from ~36617 ops/ms @ 4 threads)
- **C++ Disruptor port** (`JMH_MultiProducerSingleConsumer_producing/threads:4_mean`): **~23.62 ns/op/thread** → **~4.23e+07 ops/s per thread**
  - Approx total throughput (rule-of-thumb): **~1.69e+08 ops/s** (\(\approx 4 \times 10^9 / 23.62\))

### Multi Producer / Single Consumer (batch publish)

- **Java Disruptor** (`MultiProducerSingleConsumer.producingBatch`, thrpt): **~2.11e+08 ops/s**
- **C++ Disruptor port** (`JMH_MultiProducerSingleConsumer_producingBatch/threads:4_mean`): **~2.03e+08 items/s** (from `items_per_second`)

Interpretation:
- **MP1C batch is already close** between Java and C++ on this data set (same order of magnitude, within ~a few percent).

### Blocking queue baseline

- **Java** (`BlockingQueueBenchmark.producing`, avgt): **~123 ns/op** → **~8.12e+06 ops/s**
- **C++** (`JMH_BlockingQueueBenchmark_producing_mean`): **~680 ns/op** → **~1.47e+06 ops/s**

This baseline is meant as a “traditional blocking queue” comparator. If C++ and Java baselines differ greatly, validate that the C++ baseline matches the Java `ArrayBlockingQueue` behavior closely (fairness, wakeup strategy, object reuse, etc.).

## Caveats for interpreting cross-language numbers

- **Different harnesses**: Google Benchmark vs JMH (forked JVM, different warmup model). We align parameters, but they are still not identical runtimes.
- **OS/CPU differences**: Always compare results collected on the same machine if you want to attribute deltas to implementation differences.
- **Thread scheduling**: MP benchmarks are sensitive to core pinning, background load, and timer resolution.

## What to do next (optimization direction)

- **Prioritize SPSC**: It is currently the largest gap (C++ ~0.36x Java in ops/s in the aligned 1P1C publish test).
- **Use the included baselines**:
  - `JMH_EmptyLoop_DoNotOptimize` helps bound measurement noise.
  - `JMH_TBusSingleProducerSingleConsumer_producing` helps separate “sequencer/disruptor overhead” from “minimal ring mechanics”.
