#pragma once
// 1:1 port of com.lmax.disruptor.MultiProducerSequencer
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/MultiProducerSequencer.java

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

class MultiProducerSequencer final : public AbstractSequencer {
public:
  MultiProducerSequencer(int bufferSize, std::shared_ptr<WaitStrategy> waitStrategy)
      : AbstractSequencer(bufferSize, std::move(waitStrategy)),
        gatingSequenceCache_(Sequencer::INITIAL_CURSOR_VALUE),
        availableBuffer_(static_cast<size_t>(bufferSize)),
        indexMask_(bufferSize - 1),
        indexShift_(disruptor::util::Util::log2(bufferSize)) {
    for (auto& a : availableBuffer_) {
      a.store(-1, std::memory_order_relaxed);
    }
  }

  bool hasAvailableCapacity(int requiredCapacity) override {
    auto snap = gatingSequences_.load(std::memory_order_acquire);
    // Java passes gatingSequences array + cursor.get()
    return hasAvailableCapacity(snap.get(), requiredCapacity, cursor_.get());
  }

  void claim(int64_t sequence) override { cursor_.set(sequence); }

  int64_t next() override { return next(1); }

  int64_t next(int n) override {
    if (n < 1 || n > bufferSize_) {
      throw std::invalid_argument("n must be > 0 and < bufferSize");
    }

    int64_t current = cursor_.getAndAdd(n);
    int64_t nextSequence = current + n;
    int64_t wrapPoint = nextSequence - bufferSize_;
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

  int64_t tryNext() override { return tryNext(1); }

  int64_t tryNext(int n) override {
    if (n < 1) {
      throw std::invalid_argument("n must be > 0");
    }

    int64_t current;
    int64_t next;
    do {
      current = cursor_.get();
      next = current + n;

      auto snap = gatingSequences_.load(std::memory_order_acquire);
      if (!hasAvailableCapacity(snap.get(), n, current)) {
        throw InsufficientCapacityException::INSTANCE();
      }
    } while (!cursor_.compareAndSet(current, next));

    return next;
  }

  int64_t remainingCapacity() override {
    int64_t consumed = minimumSequence(cursor_.get());
    int64_t produced = cursor_.get();
    return getBufferSize() - (produced - consumed);
  }

  void publish(int64_t sequence) override {
    setAvailable(sequence);
    waitStrategy_->signalAllWhenBlocking();
  }

  void publish(int64_t lo, int64_t hi) override {
    for (int64_t l = lo; l <= hi; ++l) {
      setAvailable(l);
    }
    waitStrategy_->signalAllWhenBlocking();
  }

  bool isAvailable(int64_t sequence) override {
    int index = calculateIndex(sequence);
    int flag = calculateAvailabilityFlag(sequence);
    return availableBuffer_[static_cast<size_t>(index)].load(std::memory_order_acquire) == flag;
  }

  int64_t getHighestPublishedSequence(int64_t lowerBound, int64_t availableSequence) override {
    for (int64_t sequence = lowerBound; sequence <= availableSequence; ++sequence) {
      if (!isAvailable(sequence)) {
        return sequence - 1;
      }
    }
    return availableSequence;
  }

private:
  Sequence gatingSequenceCache_;
  std::vector<std::atomic<int>> availableBuffer_;
  int indexMask_;
  int indexShift_;

  bool hasAvailableCapacity(const std::vector<Sequence*>* gatingSequences, int requiredCapacity, int64_t cursorValue) {
    int64_t wrapPoint = (cursorValue + requiredCapacity) - bufferSize_;
    int64_t cachedGatingSequence = gatingSequenceCache_.get();

    if (wrapPoint > cachedGatingSequence || cachedGatingSequence > cursorValue) {
      int64_t minSequence = gatingSequences
                                ? disruptor::util::Util::getMinimumSequence(*gatingSequences, cursorValue)
                                : cursorValue;
      gatingSequenceCache_.set(minSequence);

      if (wrapPoint > minSequence) {
        return false;
      }
    }

    return true;
  }

  void setAvailable(int64_t sequence) {
    setAvailableBufferValue(calculateIndex(sequence), calculateAvailabilityFlag(sequence));
  }

  void setAvailableBufferValue(int index, int flag) {
    availableBuffer_[static_cast<size_t>(index)].store(flag, std::memory_order_release);
  }

  int calculateAvailabilityFlag(int64_t sequence) const { return static_cast<int>(sequence >> indexShift_); }
  int calculateIndex(int64_t sequence) const { return static_cast<int>(sequence) & indexMask_; }

  int64_t minimumSequence(int64_t defaultMin) {
    auto snap = gatingSequences_.load(std::memory_order_acquire);
    if (!snap) {
      return defaultMin;
    }
    return disruptor::util::Util::getMinimumSequence(*snap, defaultMin);
  }
};

} // namespace disruptor
