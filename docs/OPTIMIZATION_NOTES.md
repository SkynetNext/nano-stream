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

## Notes / caveats

- These changes are performance-oriented and should be validated against the test suite (`nano_stream_tests`) and the benchmark suite (`nano_stream_benchmarks`).
- Cross-language comparisons must still be interpreted carefully (OS scheduler differences, CPU frequency behavior, benchmark harness differences).


