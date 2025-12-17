# Optimization Notes (internal)

This document is intentionally **non-normative**. It records pragmatic optimizations made during benchmark alignment work, so we can reproduce/justify changes later.

## Background

We align a subset of C++ benchmarks (Google Benchmark) with the Java JMH benchmarks from `reference/disruptor/`.
During this work we observed that C++ SPSC could lag Java and be more jittery on some platforms. Two changes had measurable impact:

- **Padding / cache-line isolation** for hot fields (false sharing reduction)
- **Spin-wait hint + fence alignment** (closer to Java semantics and runtime behavior)

## 1) Padding to reduce false sharing

### Java reference

- `reference/disruptor/src/main/java/com/lmax/disruptor/Sequence.java`
  - Uses `LhsPadding` / `Value` / `RhsPadding` superclasses to pad around the hot `value` field.
- `reference/disruptor/src/main/java/com/lmax/disruptor/SingleProducerSequencer.java`
  - Uses `SingleProducerSequencerPad` / `SingleProducerSequencerFields` plus additional trailing padding in the concrete class.

### C++ change

- `include/disruptor/Sequence.h`
  - Mirror the Java padding intent by placing the atomic `value_` between **LHS** and **RHS** padding blocks.
- `include/disruptor/SingleProducerSequencer.h`
  - Mirror Java structure: `Pad -> Fields` and add trailing padding in `SingleProducerSequencer`.

### Why it helps

On typical x86 cache-line sizes, padding reduces the chance that frequently-updated values share a cache line with other frequently-updated fields, which can otherwise amplify coherence traffic and jitter.

## 2) Spin-wait hint + setVolatile fence alignment

### Java reference

- `reference/disruptor/src/main/java/com/lmax/disruptor/util/ThreadHints.java`
  - `onSpinWait()` delegates to `Thread.onSpinWait()`.
- `reference/disruptor/src/main/java/com/lmax/disruptor/Sequence.java`
  - `setVolatile()` is:
    - `VarHandle.releaseFence();`
    - `this.value = value;`
    - `VarHandle.fullFence();`

### C++ change

- `include/disruptor/util/ThreadHints.h`
  - Implement `ThreadHints::onSpinWait()` using `_mm_pause()` on x86/x64.
  - Fallback to `std::this_thread::yield()` on non-x86 platforms.
- `include/disruptor/Sequence.h`
  - Implement `Sequence::setVolatile()` as:
    - `atomic_thread_fence(release)`
    - `store(relaxed)`
    - `atomic_thread_fence(seq_cst)` (full fence intent)

### Why it helps

- `_mm_pause()` is a better approximation of Java `Thread.onSpinWait()` than `yield()`: it is a spin hint rather than a scheduler hint.
- The `setVolatile` fence pattern more closely matches the Java implementation, and can avoid unintended costs/semantics from a single `seq_cst` store.

### What we observed in practice

- **Padding was the primary contributor** to closing the SPSC gap and reducing jitter.
- **Spin/fence alignment produced smaller, incremental improvements** (mainly stability and a small ns/op reduction), but is still worth keeping for semantic alignment with the Java reference.

The current baseline CI comparison we reference is the aligned run generated on **2025-12-17 11:09 UTC** (see `comparison_report.md` in CI artifacts).

## Perf-driven diagnosis (SPSC)

### Build used for profiling

- `cmake -S . -B build-perf -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=gcc-12 -DCMAKE_CXX_COMPILER=g++-12`
- `cmake --build build-perf -j`
- `./build-perf/benchmarks/nano_stream_benchmarks --benchmark_filter='^JMH_SingleProducerSingleConsumer_producing($|/.*)' --benchmark_min_warmup_time=10 --benchmark_min_time=5s --benchmark_repetitions=3 --benchmark_report_aggregates_only=true`

### Perf workflow (Linux)

- Record (recommend `cycles:u` and moderate sampling frequency to avoid lost samples):
  - `sudo perf record -e cycles:u -F 999 -g --call-graph=dwarf -- ./build-perf/benchmarks/nano_stream_benchmarks --benchmark_filter='^JMH_SingleProducerSingleConsumer_producing($|/.*)' --benchmark_min_warmup_time=10 --benchmark_min_time=5s --benchmark_repetitions=3 --benchmark_report_aggregates_only=true`
- Export text reports:
  - `sudo perf report --stdio --no-children --percent-limit 0.5 > perf_cycles.txt`
  - `sudo perf report --stdio -g graph,0.5,caller --percent-limit 0.5 > perf_cycles_callgraph.txt`

### What perf showed (actionable hotspots)

- **Producer hot path** had non-trivial overhead in `RingBuffer::publish` due to
  `std::shared_ptr<disruptor::Sequencer>::get/operator->`.
  - This is an avoidable tax on the critical path; Java typically devirtualizes/optimizes field access here.
- **Producer also paid** for `BusySpinWaitStrategy::signalAllWhenBlocking` being invoked on every publish.
  - Java reference confirms this is an interface call (`WaitStrategy.signalAllWhenBlocking()`), but for `BusySpinWaitStrategy`
    the implementation is `final` and the method body is empty, so the JVM can devirtualize + inline it away in practice.
- **Consumer busy-spin** naturally dominates CPU time in SPSC when using `BusySpinWaitStrategy`:
  - `BusySpinWaitStrategy::waitFor` and `Sequence::get` show up heavily; this is expected and is not, by itself, evidence
    that the producer path is slow.

### Changes made based on profiling & measurements

- **Padding alignment (primary impact)**:
  - Mirror Java padding structure in `Sequence` and `SingleProducerSequencer` to reduce false sharing.
- **Spin/fence alignment (incremental impact)**:
  - `ThreadHints::onSpinWait()` uses `_mm_pause()` on x86/x64.
  - `Sequence::setVolatile()` matches Java's `releaseFence + store + fullFence` intent via fences + relaxed store.
- **Blocking queue baseline**:
  - Fix `offer()` fast-path to avoid `condition_variable::wait_for` when not full.
  - Reduce `notify_one()` calls to only trigger on empty→non-empty and full→not-full transitions.
- **tbus benchmark correctness**:
  - Replace the “tbus-inspired” ring with a benchmark that calls the original TSF4G `tbus` API directly on Linux.

## Next planned optimization (based on perf)

- Avoid `shared_ptr` on the SPSC critical path:
  - Keep ownership with `shared_ptr`, but use a cached `Sequencer*` (raw pointer) for hot `next/publish` calls to eliminate
    `shared_ptr_access` overhead in `RingBuffer::publish`.

## Notes / caveats

- These changes are performance-oriented and should be validated against the test suite (`nano_stream_tests`) and the benchmark suite (`nano_stream_benchmarks`).
- Cross-language comparisons must still be interpreted carefully (OS scheduler differences, CPU frequency behavior, benchmark harness differences).


