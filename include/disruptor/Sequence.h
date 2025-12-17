#pragma once
// 1:1 port skeleton of com.lmax.disruptor.Sequence

#include <atomic>
#include <cstdint>

namespace disruptor {

class Sequence {
public:
  static constexpr int64_t INITIAL_VALUE = -1;

  Sequence() noexcept : value_(INITIAL_VALUE) {}
  explicit Sequence(int64_t initial) noexcept : value_(initial) {}
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

private:
  std::atomic<int64_t> value_;
};

} // namespace disruptor


