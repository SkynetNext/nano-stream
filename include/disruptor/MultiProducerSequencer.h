#pragma once
// 1:1 port of com.lmax.disruptor.MultiProducerSequencer
// Source:
// reference/disruptor/src/main/java/com/lmax/disruptor/MultiProducerSequencer.java

#include "AbstractSequencer.h"
#include "InsufficientCapacityException.h"
#include "Sequence.h"
#include "WaitStrategy.h"
#include "util/Util.h"

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <vector>

namespace disruptor {

template <typename WaitStrategyT>
class MultiProducerSequencer final : public AbstractSequencer<WaitStrategyT> {
public:
  MultiProducerSequencer(int bufferSize, WaitStrategyT waitStrategy)
      : AbstractSequencer<WaitStrategyT>(bufferSize, std::move(waitStrategy)),
        gatingSequenceCache_(SEQUENCER_INITIAL_CURSOR_VALUE),
        availableBuffer_(static_cast<size_t>(bufferSize)),
        indexMask_(bufferSize - 1),
        indexShift_(disruptor::util::Util::log2(bufferSize)),
        gatingSequencesCache_(nullptr) {
    for (auto &a : availableBuffer_) {
      a.store(-1, std::memory_order_relaxed);
    }
  }

  bool hasAvailableCapacity(int requiredCapacity) {
    auto snap = this->gatingSequences_.load(std::memory_order_acquire);
    // Java passes gatingSequences array + cursor.get()
    return hasAvailableCapacity(snap.get(), requiredCapacity,
                                this->cursor_.get());
  }

  void claim(int64_t sequence) { this->cursor_.set(sequence); }

  int64_t next() { return next(1); }

  int64_t next(int n) {
    if (n < 1 || n > this->bufferSize_) {
      throw std::invalid_argument("n must be > 0 and < bufferSize");
    }

    int64_t current = this->cursor_.getAndAdd(n);
    int64_t nextSequence = current + n;
    int64_t wrapPoint = nextSequence - this->bufferSize_;
    int64_t cachedGatingSequence = gatingSequenceCache_.get();

    if (wrapPoint > cachedGatingSequence || cachedGatingSequence > current) {
      int64_t gatingSequence;
      while (wrapPoint > (gatingSequence = minimumSequence(current))) {
        std::this_thread::yield();
      }
      gatingSequenceCache_.set(gatingSequence);
    }

    return nextSequence;
  }

  int64_t tryNext() { return tryNext(1); }

  int64_t tryNext(int n) {
    if (n < 1) {
      throw std::invalid_argument("n must be > 0");
    }

    int64_t current;
    int64_t next;
    do {
      current = this->cursor_.get();
      next = current + n;

      auto snap = this->gatingSequences_.load(std::memory_order_acquire);
      if (!hasAvailableCapacity(snap.get(), n, current)) {
        throw InsufficientCapacityException::INSTANCE();
      }
    } while (!this->cursor_.compareAndSet(current, next));

    return next;
  }

  int64_t remainingCapacity() {
    int64_t consumed = minimumSequence(this->cursor_.get());
    int64_t produced = this->cursor_.get();
    return this->getBufferSize() - (produced - consumed);
  }

  void publish(int64_t sequence) {
    setAvailable(sequence);
    if constexpr (WaitStrategyT::kIsBlockingStrategy) {
      this->waitStrategy_.signalAllWhenBlocking();
    }
  }

  void publish(int64_t lo, int64_t hi) {
    for (int64_t l = lo; l <= hi; ++l) {
      setAvailable(l);
    }
    if constexpr (WaitStrategyT::kIsBlockingStrategy) {
      this->waitStrategy_.signalAllWhenBlocking();
    }
  }

  bool isAvailable(int64_t sequence) {
    int index = calculateIndex(sequence);
    int flag = calculateAvailabilityFlag(sequence);
    return availableBuffer_[static_cast<size_t>(index)].load(
               std::memory_order_acquire) == flag;
  }

  int64_t getHighestPublishedSequence(int64_t lowerBound,
                                      int64_t availableSequence) {
    for (int64_t sequence = lowerBound; sequence <= availableSequence;
         ++sequence) {
      if (!isAvailable(sequence)) {
        return sequence - 1;
      }
    }
    return availableSequence;
  }

  // Override to invalidate cache when gating sequences change
  void addGatingSequences(Sequence *const *gatingSequences, int count) {
    AbstractSequencer<WaitStrategyT>::addGatingSequences(gatingSequences,
                                                         count);
    gatingSequencesCache_ = nullptr; // Invalidate cache
  }

  bool removeGatingSequence(Sequence &sequence) {
    bool result =
        AbstractSequencer<WaitStrategyT>::removeGatingSequence(sequence);
    gatingSequencesCache_ = nullptr; // Invalidate cache
    return result;
  }

private:
  Sequence gatingSequenceCache_;
  std::vector<std::atomic<int>> availableBuffer_;
  int indexMask_;
  int indexShift_;
  // Optimization: Cache raw pointer to gatingSequences vector to avoid atomic
  // shared_ptr operations. This is safe because gatingSequences_ is only
  // updated during add/remove (not on hot path), and we refresh the cache when
  // it's null.
  mutable const std::vector<Sequence *> *gatingSequencesCache_;

  bool hasAvailableCapacity(const std::vector<Sequence *> *gatingSequences,
                            int requiredCapacity, int64_t cursorValue) {
    int64_t wrapPoint = (cursorValue + requiredCapacity) - this->bufferSize_;
    int64_t cachedGatingSequence = gatingSequenceCache_.get();

    if (wrapPoint > cachedGatingSequence ||
        cachedGatingSequence > cursorValue) {
      int64_t minSequence = gatingSequences
                                ? disruptor::util::Util::getMinimumSequence(
                                      *gatingSequences, cursorValue)
                                : cursorValue;
      gatingSequenceCache_.set(minSequence);

      if (wrapPoint > minSequence) {
        return false;
      }
    }

    return true;
  }

  void setAvailable(int64_t sequence) {
    setAvailableBufferValue(calculateIndex(sequence),
                            calculateAvailabilityFlag(sequence));
  }

  void setAvailableBufferValue(int index, int flag) {
    availableBuffer_[static_cast<size_t>(index)].store(
        flag, std::memory_order_release);
  }

  int calculateAvailabilityFlag(int64_t sequence) const {
    return static_cast<int>(sequence >> indexShift_);
  }
  int calculateIndex(int64_t sequence) const {
    return static_cast<int>(sequence) & indexMask_;
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
      auto snap = this->gatingSequences_.load(std::memory_order_acquire);
      if (!snap) {
        return defaultMin;
      }
      gatingSequencesCache_ = snap.get();
      return disruptor::util::Util::getMinimumSequence(*snap, defaultMin);
    }
    return disruptor::util::Util::getMinimumSequence(*cached, defaultMin);
  }
};

} // namespace disruptor
