#pragma once
// 1:1 port skeleton of com.lmax.disruptor.Sequence

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace disruptor {

// Java reference (for padding intent):
//   reference/disruptor/src/main/java/com/lmax/disruptor/Sequence.java
// Java uses padding superclasses to reduce false sharing around the hot `value` field.
// We mirror the structure in C++.
namespace detail {

// Java: Sequence.INITIAL_VALUE = -1L
inline constexpr int64_t kInitialValue = -1;

// 7 * 16 bytes = 112 bytes, matching the Java source layout intent (p10..p77).
struct LhsPadding {
  std::byte p1[112]{};
};

struct Value : LhsPadding {
  std::atomic<int64_t> value_;
  Value() noexcept : value_(kInitialValue) {}
  explicit Value(int64_t initial) noexcept : value_(initial) {}
};

// Another 112 bytes on the RHS (p90..p157 in Java).
struct RhsPadding : Value {
  std::byte p2[112]{};
  RhsPadding() noexcept : Value() {}
  explicit RhsPadding(int64_t initial) noexcept : Value(initial) {}
};

} // namespace detail

class Sequence : public detail::RhsPadding {
public:
  static constexpr int64_t INITIAL_VALUE = -1;

  Sequence() noexcept : detail::RhsPadding(INITIAL_VALUE) {}
  explicit Sequence(int64_t initial) noexcept : detail::RhsPadding(initial) {}
  virtual ~Sequence() = default;

  // NOTE: Do not mark these virtual methods noexcept.
  // Some derived Sequence types (e.g. FixedSequenceGroup) intentionally throw
  // UnsupportedOperationException equivalents. C++ forbids widening exception
  // specifications in overrides, so the base must not be noexcept here.
  virtual int64_t get() const { return value_.load(std::memory_order_acquire); }
  virtual void set(int64_t v) { value_.store(v, std::memory_order_release); }

  // Java: setVolatile - used as StoreLoad fence.
  virtual void setVolatile(int64_t v) { value_.store(v, std::memory_order_seq_cst); }

  virtual bool compareAndSet(int64_t expected, int64_t desired) {
    return value_.compare_exchange_strong(expected, desired,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire);
  }

  virtual int64_t incrementAndGet() {
    return value_.fetch_add(1, std::memory_order_acq_rel) + 1;
  }

  virtual int64_t addAndGet(int64_t increment) {
    return value_.fetch_add(increment, std::memory_order_acq_rel) + increment;
  }

  virtual int64_t getAndAdd(int64_t increment) {
    return value_.fetch_add(increment, std::memory_order_acq_rel);
  }
};

} // namespace disruptor


