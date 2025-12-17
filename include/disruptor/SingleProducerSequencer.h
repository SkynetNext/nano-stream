#pragma once
// 1:1 port of com.lmax.disruptor.SingleProducerSequencer
// Source:
// reference/disruptor/src/main/java/com/lmax/disruptor/SingleProducerSequencer.java

#include "AbstractSequencer.h"
#include "InsufficientCapacityException.h"
#include "Sequence.h"
#include "WaitStrategy.h"
#include "util/Util.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>

namespace disruptor {

// Diagnostic counters used by benchmarks to detect producer backpressure (wrap
// wait) in SPSC. These are intentionally relaxed atomics; they are not part of
// the Disruptor API contract.
inline std::atomic<uint64_t> &sp_wrap_wait_entries() {
  static std::atomic<uint64_t> v{0};
  return v;
}
inline std::atomic<uint64_t> &sp_wrap_wait_loops() {
  static std::atomic<uint64_t> v{0};
  return v;
}

// Java reference (padding intent):
//   reference/disruptor/src/main/java/com/lmax/disruptor/SingleProducerSequencer.java
// Java uses padding superclasses to reduce false sharing around sequencer hot
// fields.
namespace detail {

// 112 bytes of padding (same shape as Java p10..p77).
struct SpSequencerPad : public AbstractSequencer {
  std::byte p1[112]{};
  SpSequencerPad(int bufferSize, std::unique_ptr<WaitStrategy> waitStrategy)
      : AbstractSequencer(bufferSize, std::move(waitStrategy)) {}
};

struct SpSequencerFields : public SpSequencerPad {
  int64_t nextValue_;
  int64_t cachedValue_;
  SpSequencerFields(int bufferSize, std::unique_ptr<WaitStrategy> waitStrategy)
      : SpSequencerPad(bufferSize, std::move(waitStrategy)),
        nextValue_(Sequence::INITIAL_VALUE),
        cachedValue_(Sequence::INITIAL_VALUE) {}
};

} // namespace detail

class SingleProducerSequencer final : public detail::SpSequencerFields {
public:
  SingleProducerSequencer(int bufferSize,
                          std::unique_ptr<WaitStrategy> waitStrategy)
      : detail::SpSequencerFields(bufferSize, std::move(waitStrategy)),
        gatingSequencesCache_(nullptr) {}

  bool hasAvailableCapacity(int requiredCapacity) override {
    return hasAvailableCapacity(requiredCapacity, false);
  }

  int64_t next() override { return next(1); }

  int64_t next(int n) override {
    // Java: assert sameThread() when assertions enabled. We keep a debug check.
    // IMPORTANT: This must be debug-only. A per-call mutex/map check destroys
    // SPSC performance and Java only performs this when assertions are enabled.
#ifndef NDEBUG
    if (!sameThread()) {
      throw std::runtime_error(
          "Accessed by two threads - use ProducerType.MULTI!");
    }
#endif
    if (n < 1 || n > bufferSize_) {
      throw std::invalid_argument("n must be > 0 and < bufferSize");
    }

    int64_t nextValue = nextValue_;
    int64_t nextSequence = nextValue + n;
    int64_t wrapPoint = nextSequence - bufferSize_;
    int64_t cachedGatingSequence = cachedValue_;

    if (wrapPoint > cachedGatingSequence || cachedGatingSequence > nextValue) {
      sp_wrap_wait_entries().fetch_add(1, std::memory_order_relaxed);
      cursor_.setVolatile(nextValue); // StoreLoad fence

      int64_t minSequence;
      while (wrapPoint > (minSequence = minimumSequence(nextValue))) {
        sp_wrap_wait_loops().fetch_add(1, std::memory_order_relaxed);
        // Java: LockSupport.parkNanos(1L)
        std::this_thread::yield();
      }

      cachedValue_ = minSequence;
    }

    nextValue_ = nextSequence;
    return nextSequence;
  }

  int64_t tryNext() override { return tryNext(1); }

  int64_t tryNext(int n) override {
    if (n < 1) {
      throw std::invalid_argument("n must be > 0");
    }

    if (!hasAvailableCapacity(n, true)) {
      throw InsufficientCapacityException::INSTANCE();
    }

    nextValue_ += n;
    return nextValue_;
  }

  int64_t remainingCapacity() override {
    int64_t nextValue = nextValue_;
    int64_t consumed = minimumSequence(nextValue);
    int64_t produced = nextValue;
    return getBufferSize() - (produced - consumed);
  }

  void claim(int64_t sequence) override { nextValue_ = sequence; }

  void publish(int64_t sequence) override {
    cursor_.set(sequence);
    // Optimization: For non-blocking strategies (e.g. BusySpinWaitStrategy),
    // signalOnPublish_ is false. Use [[unlikely]] to hint the compiler that
    // this branch is rarely taken in hot paths (SPSC).
    if (signalOnPublish_) [[unlikely]] {
      waitStrategy_->signalAllWhenBlocking();
    }
  }

  void publish(int64_t lo, int64_t hi) override { publish(hi); }

  bool isAvailable(int64_t sequence) override {
    const int64_t currentSequence = cursor_.get();
    return sequence <= currentSequence &&
           sequence > currentSequence - bufferSize_;
  }

  int64_t getHighestPublishedSequence(int64_t /*lowerBound*/,
                                      int64_t availableSequence) override {
    return availableSequence;
  }

  // Override to invalidate cache when gating sequences change
  void addGatingSequences(Sequence *const *gatingSequences,
                          int count) override {
    AbstractSequencer::addGatingSequences(gatingSequences, count);
    gatingSequencesCache_ = nullptr; // Invalidate cache
  }

  bool removeGatingSequence(Sequence &sequence) override {
    bool result = AbstractSequencer::removeGatingSequence(sequence);
    gatingSequencesCache_ = nullptr; // Invalidate cache
    return result;
  }

private:
  // Optimization: Cache raw pointer to gatingSequences vector to avoid atomic
  // shared_ptr operations. This is safe because gatingSequences_ is only
  // updated during add/remove (not on hot path), and we refresh the cache when
  // it's null.
  mutable const std::vector<Sequence *> *gatingSequencesCache_;

  // Trailing padding to mirror Java's extra padding in the concrete class.
  std::byte p2_[112 - sizeof(const std::vector<Sequence *> *)]{};

  bool hasAvailableCapacity(int requiredCapacity, bool doStore) {
    int64_t nextValue = nextValue_;
    int64_t wrapPoint = (nextValue + requiredCapacity) - bufferSize_;
    int64_t cachedGatingSequence = cachedValue_;

    if (wrapPoint > cachedGatingSequence || cachedGatingSequence > nextValue) {
      if (doStore) {
        cursor_.setVolatile(nextValue); // StoreLoad fence
      }

      int64_t minSequence = minimumSequence(nextValue);
      cachedValue_ = minSequence;

      if (wrapPoint > minSequence) {
        return false;
      }
    }

    return true;
  }

  int64_t minimumSequence(int64_t defaultMin) {
    // Optimization: Cache raw pointer to avoid shared_ptr atomic operations on
    // hot path. gatingSequences_ is updated rarely (only when consumers are
    // added/removed), but accessed frequently in next(). Use cached pointer for
    // fast path.
    auto *cached = gatingSequencesCache_;
    if (!cached) {
      // Fallback: reload if cache is null (initialization or after
      // modifications)
      auto snap = gatingSequences_.load(std::memory_order_acquire);
      if (!snap) {
        return defaultMin;
      }
      gatingSequencesCache_ = snap.get();
      return disruptor::util::Util::getMinimumSequence(*snap, defaultMin);
    }
    return disruptor::util::Util::getMinimumSequence(*cached, defaultMin);
  }

  // Java-only debug assertion; best-effort in C++.
  bool sameThread() {
#ifdef NDEBUG
    return true;
#else
    static std::mutex m;
    static std::unordered_map<const SingleProducerSequencer *, std::thread::id>
        producers;
    std::lock_guard<std::mutex> lock(m);
    const auto tid = std::this_thread::get_id();
    auto it = producers.find(this);
    if (it == producers.end()) {
      producers.emplace(this, tid);
      return true;
    }
    return it->second == tid;
#endif
  }
};

} // namespace disruptor
