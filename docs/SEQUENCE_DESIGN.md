# Sequence Design: Padding and VarHandle

## 1. False Sharing Problem

**False Sharing** occurs when two threads access different variables that happen to be in the same **cache line** (typically 64 bytes). When one thread writes, it invalidates the entire cache line, forcing other threads to reload from memory even though their variable wasn't modified.

In Disruptor, multiple threads access different `Sequence` objects (producer cursor, consumer sequences). If their `value` fields share a cache line, performance degrades significantly.

## 2. Padding Solution

### Implementation

**Java:**
```java
class LhsPadding { protected byte p10...p77; }  // 112 bytes
class Value extends LhsPadding { protected long value; }  // 8 bytes
class RhsPadding extends Value { protected byte p90...p157; }  // 112 bytes
public class Sequence extends RhsPadding { ... }
```

**C++:**
```cpp
struct LhsPadding { std::byte p1[112]{}; };
struct Value : LhsPadding { std::atomic<int64_t> value_; };
struct RhsPadding : Value { std::byte p2[112]{}; };
class Sequence : public RhsPadding { ... };
```

**Memory Layout:** `[112 bytes padding] [8 bytes value] [112 bytes padding] = 232 bytes total`

### Why 112 Bytes? (Total 232 Bytes)

112 bytes of padding on each side ensures that even with object alignment variations, the `value` field (8 bytes) won't share a cache line with another object's `value`. 

Total object size (232 bytes) is significantly larger than both:
- 64 bytes (cache line size) - prevents false sharing
- 128 bytes (L2 prefetcher pair size) - may help avoid prefetcher interference when multiple `Sequence` objects are grouped together

## 3. alignas(64) vs Padding

**Key Difference:**
- `alignas(64)` only ensures **starting address** alignment (64-byte boundary)
- **Padding** ensures **object size** is large enough to prevent cache line sharing

**When `alignas(64)` alone is insufficient:**
- In arrays: Objects are still packed consecutively. If object size < 64 bytes, they can share cache lines.
- Example: `alignas(64) Sequence arr[2]` → `arr[0]` at `0x1000`, `arr[1]` at `0x1008` (same cache line if object < 64 bytes)

**Best Practice:** Use both `alignas(64)` and padding for maximum safety, though padding alone is usually sufficient if object size ≥ 64 bytes.

### 128-Byte Alignment Consideration

When a structure needs to access **two consecutive cache lines**, `alignas(128)` may provide additional optimization beyond `alignas(64)`.

**Reference:** [Stack Overflow discussion](https://stackoverflow.com/questions/63706666/false-sharing-prevention-with-alignas-is-broken) shows that Intel's L2 spatial prefetcher tries to complete 128-byte aligned pairs of cache lines. For structures accessing two consecutive cache lines, aligning the entire structure to 128 bytes can optimize prefetcher behavior.

**For Disruptor:** The 232-byte `Sequence` size naturally exceeds 128 bytes, which may provide additional benefits when multiple `Sequence` objects are grouped together.

## 4. VarHandle for Fine-Grained Memory Ordering

`VarHandle` (Java 9+) provides fine-grained memory ordering control, unlike `volatile` which has fixed acquire/release semantics.

**Java Implementation:**
```java
// get(): plain read + acquire fence (allows "slightly stale" reads, reduces overhead)
public long get() {
    long value = this.value;        // Plain read
    VarHandle.acquireFence();       // acquire fence
    return value;
}

// set(): release fence + plain write
public void set(final long value) {
    VarHandle.releaseFence();       // release fence
    this.value = value;             // Plain write
}

// setVolatile(): release fence + write + full fence (StoreLoad barrier)
public void setVolatile(final long value) {
    VarHandle.releaseFence();
    this.value = value;
    VarHandle.fullFence();
}
```

## 5. C++ Equivalent Implementation

```cpp
// get(): C++ standard practice - load(acquire)
virtual int64_t get() const {
    return value_.load(std::memory_order_acquire);
}

// set(): release semantics
virtual void set(int64_t v) {
    value_.store(v, std::memory_order_release);
}

// setVolatile(): release fence + relaxed store + full fence (matches Java)
virtual void setVolatile(int64_t v) {
    std::atomic_thread_fence(std::memory_order_release);
    value_.store(v, std::memory_order_relaxed);
    std::atomic_thread_fence(std::memory_order_seq_cst);
}
```

## 6. Performance Impact

According to `docs/OPTIMIZATION_NOTES.md`:
- **Padding is the primary contributor**: Significantly reduced SPSC performance gap and jitter
- **Spin/fence alignment**: Smaller incremental improvements (stability, small ns/op reduction)

**Effects:** Reduced cache coherence traffic, lower jitter, improved throughput.

## 7. Summary

| Technique | Purpose |
|-----------|---------|
| **Padding (112+8+112 bytes)** | Prevent false sharing by ensuring `value` occupies its own cache line |
| **alignas(64)** | Ensure starting address alignment (complements padding, not a replacement) |
| **VarHandle / relaxed+fence** | Fine-grained memory ordering to reduce barrier overhead |
| **get(): relaxed load + acquire fence** | Allow "slightly stale" reads, reducing overhead |

These optimizations are key to Disruptor's high performance.
