#pragma once
// 1:1 port of com.lmax.disruptor.SingleProducerSequencer
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/SingleProducerSequencer.java

#include "AbstractSequencer.h"
#include "InsufficientCapacityException.h"
#include "Sequence.h"
#include "WaitStrategy.h"
#include "util/Util.h"

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <mutex>

namespace disruptor {

// Java has padding superclasses; we keep minimal padding hints.
class SingleProducerSequencer final : public AbstractSequencer {
public:
  SingleProducerSequencer(int bufferSize, std::shared_ptr<WaitStrategy> waitStrategy)
      : AbstractSequencer(bufferSize, std::move(waitStrategy)),
        nextValue_(Sequence::INITIAL_VALUE),
        cachedValue_(Sequence::INITIAL_VALUE) {}

  bool hasAvailableCapacity(int requiredCapacity) override { return hasAvailableCapacity(requiredCapacity, false); }

  int64_t next() override { return next(1); }

  int64_t next(int n) override {
    // Java: assert sameThread() when assertions enabled. We keep a debug check.
    // IMPORTANT: This must be debug-only. A per-call mutex/map check destroys SPSC performance and
    // Java only performs this when assertions are enabled.
#ifndef NDEBUG
    if (!sameThread()) {
      throw std::runtime_error("Accessed by two threads - use ProducerType.MULTI!");
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
      cursor_.setVolatile(nextValue); // StoreLoad fence

      int64_t minSequence;
      while (wrapPoint > (minSequence = minimumSequence(nextValue))) {
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
    waitStrategy_->signalAllWhenBlocking();
  }

  void publish(int64_t lo, int64_t hi) override { publish(hi); }

  bool isAvailable(int64_t sequence) override {
    const int64_t currentSequence = cursor_.get();
    return sequence <= currentSequence && sequence > currentSequence - bufferSize_;
  }

  int64_t getHighestPublishedSequence(int64_t /*lowerBound*/, int64_t availableSequence) override {
    return availableSequence;
  }

private:
  int64_t nextValue_;
  int64_t cachedValue_;

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
    auto snap = gatingSequences_.load(std::memory_order_acquire);
    if (!snap) {
      return defaultMin;
    }
    return disruptor::util::Util::getMinimumSequence(*snap, defaultMin);
  }

  // Java-only debug assertion; best-effort in C++.
  bool sameThread() {
#ifdef NDEBUG
    return true;
#else
    static std::mutex m;
    static std::unordered_map<const SingleProducerSequencer*, std::thread::id> producers;
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
