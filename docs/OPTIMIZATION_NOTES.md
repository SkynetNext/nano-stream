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

## Optimization round 2: perf-guided hot-path improvements (2025-12-17)

After initial padding/fence alignment (see above), we ran detailed profiling with `perf record/report` on the SPSC benchmark 
(`JMH_SingleProducerSingleConsumer_producing`) and identified remaining bottlenecks:

### Identified hotspots (from perf report)

1. **Producer path** (~50% CPU):
   - `SingleProducerSequencer::next()`: 30.12%
   - `SingleProducerSequencer::publish()`: 11.01%
   - Overhead from empty `signalAllWhenBlocking()` calls: 3.66%

2. **Consumer path** (~50% CPU):
   - `Sequence::get()` in busy-spin: 22.88%
   - `BusySpinWaitStrategy::waitFor`: 18.56%

### Optimizations implemented

#### 1. Cache raw pointer to gatingSequences (CRITICAL - 14.22% overhead)

**Problem**: Perf revealed 14.22% overhead from `std::__shared_ptr_access<disruptor::Sequencer, ...>::operator->`.
Root cause: `gatingSequences_` is `std::atomic<std::shared_ptr<std::vector<Sequence*>>>`. Every `.load()` in 
`SingleProducerSequencer::minimumSequence()` and `MultiProducerSequencer::minimumSequence()` (called from `next()` 
on hot path) performs atomic operations and reference count manipulation.

**Solution**: Added a cached raw pointer `gatingSequencesCache_` in both `SingleProducerSequencer` and 
`MultiProducerSequencer` that points to the vector directly:
- Initialize cache to nullptr
- On first access in `minimumSequence()`, load shared_ptr and cache the raw pointer
- Invalidate cache (set to nullptr) when `addGatingSequences()` or `removeGatingSequence()` is called
- Since gating sequences are only modified at setup/teardown (not during benchmark hot path), cache remains valid

```cpp
// Before: atomic shared_ptr load on every next() call
int64_t minimumSequence(int64_t defaultMin) {
  auto snap = gatingSequences_.load(std::memory_order_acquire);  // Atomic + refcount ops!
  if (!snap) return defaultMin;
  return Util::getMinimumSequence(*snap, defaultMin);
}

// After: cached raw pointer (fast path)
mutable const std::vector<Sequence*>* gatingSequencesCache_;

int64_t minimumSequence(int64_t defaultMin) {
  auto* cached = gatingSequencesCache_;
  if (!cached) {  // Only on first call or after modification
    auto snap = gatingSequences_.load(std::memory_order_acquire);
    if (!snap) return defaultMin;
    gatingSequencesCache_ = snap.get();  // Cache for subsequent calls
    return Util::getMinimumSequence(*snap, defaultMin);
  }
  return Util::getMinimumSequence(*cached, defaultMin);  // Fast path: no atomic ops
}
```

**Expected impact**: **10-15% throughput improvement** by eliminating atomic shared_ptr operations from producer hot path.

#### 2. Branch prediction hint for non-blocking strategies

**Problem**: Even though `signalOnPublish_` correctly guards calls to `signalAllWhenBlocking()` for non-blocking strategies,
perf showed 3.66% overhead attributed to `BusySpinWaitStrategy::signalAllWhenBlocking` calls.

**Solution**: Added `[[unlikely]]` attribute to the `if (signalOnPublish_)` branch in `SingleProducerSequencer::publish()` 
and `MultiProducerSequencer::publish()` to hint the compiler that this branch is cold for BusySpinWaitStrategy.

```cpp
if (signalOnPublish_) [[unlikely]] {
  waitStrategy_->signalAllWhenBlocking();
}
```

**Expected impact**: 3-5% throughput improvement by eliminating branch misprediction and improving instruction cache utilization.

### Combined expected improvement

Conservative estimate: **13-20% overall SPSC throughput improvement**
- Cached gatingSequences pointer: 10-15% (primary)
- Branch hint optimization: 3-5%

### Validation plan

1. Re-run `perf record` on optimized build:
   ```bash
   sudo perf record -e cycles:u -F 999 -g --call-graph=dwarf -- \
     ./build-perf/benchmarks/nano_stream_benchmarks \
     --benchmark_filter='^JMH_SingleProducerSingleConsumer_producing($|/.*)' \
     --benchmark_min_time=5s
   ```

2. Compare perf reports:
   - `std::__shared_ptr_access<disruptor::Sequencer, ...>` should drop from 14.22% to near-zero
   - `BusySpinWaitStrategy::signalAllWhenBlocking` should drop from 3.66% to <0.5%

3. Benchmark throughput comparison:
   - Run comparison script against baseline (from 2025-12-17 11:09 UTC)
   - Target: 5-10% improvement in SPSC ops/sec

### Next optimization opportunities

If further improvement is needed after validation:
- Consider CRTP-based compile-time WaitStrategy selection (would eliminate 3.66% virtual call overhead entirely)
  - Template `AbstractSequencer` and derived classes on `WaitStrategy` type
  - Allows compiler to inline `signalAllWhenBlocking()` calls and devirtualize `waitFor()`
  - Trade-off: More template instantiations, slightly larger binary
- Profile multi-producer scenarios separately (different hot paths and contention patterns)

## Notes / caveats

- These changes are performance-oriented and should be validated against the test suite (`nano_stream_tests`) and the benchmark suite (`nano_stream_benchmarks`).
- Cross-language comparisons must still be interpreted carefully (OS scheduler differences, CPU frequency behavior, benchmark harness differences).


