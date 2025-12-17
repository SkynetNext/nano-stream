#pragma once
// 1:1 port of com.lmax.disruptor.AbstractSequencer
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/AbstractSequencer.java

#include "Sequence.h"
#include "Sequencer.h"
#include "WaitStrategy.h"

#include "SequenceGroups.h"
#include "util/Util.h"
#include "EventPoller.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace disruptor {

template <typename SequencerT, typename WaitStrategyT> class ProcessingSequenceBarrier;

template <typename T> class DataProvider;
template <typename T> class EventPoller;

// Java uses AtomicReferenceFieldUpdater over a volatile Sequence[] for gatingSequences.
// C++ port uses an atomic shared_ptr snapshot updated via CAS (see SequenceGroups).
//
// Template version: WaitStrategy is stored by value and statically dispatched.
template <typename WaitStrategyT>
class AbstractSequencer {
public:
  AbstractSequencer(int bufferSize, WaitStrategyT waitStrategy)
      : bufferSize_(bufferSize),
        waitStrategy_(std::move(waitStrategy)),
        cursor_(Sequencer::INITIAL_CURSOR_VALUE),
        gatingSequences_(std::make_shared<std::vector<Sequence*>>()) {
    if (bufferSize < 1) {
      throw std::invalid_argument("bufferSize must not be less than 1");
    }
    if ((bufferSize & (bufferSize - 1)) != 0) {
      throw std::invalid_argument("bufferSize must be a power of 2");
    }
  }

  int64_t getCursor() const { return cursor_.get(); }
  int getBufferSize() const { return bufferSize_; }

  void addGatingSequences(Sequence* const* gatingSequences, int count) {
    SequenceGroups::addSequences(*this, gatingSequences_, *this, gatingSequences, count);
  }

  bool removeGatingSequence(Sequence& sequence) {
    return SequenceGroups::removeSequence(*this, gatingSequences_, sequence);
  }

  int64_t getMinimumSequence() {
    auto snap = gatingSequences_.load(std::memory_order_acquire);
    if (!snap) {
      return cursor_.get();
    }
    return disruptor::util::Util::getMinimumSequence(*snap, cursor_.get());
  }

  std::shared_ptr<ProcessingSequenceBarrier<AbstractSequencer<WaitStrategyT>, WaitStrategyT>>
  newBarrier(Sequence* const* sequencesToTrack, int count);

  template <typename T>
  std::shared_ptr<EventPoller<T>> newPoller(DataProvider<T>& dataProvider, Sequence* const* gatingSequences, int count) {
    // Java: return EventPoller.newInstance(dataProvider, this, new Sequence(), cursor, gatingSequences);
    auto pollerSequence = std::make_shared<Sequence>();
    return EventPoller<T>::newInstance(dataProvider, *this, std::move(pollerSequence), cursor_, gatingSequences, count);
  }

  std::string toString() const {
    return std::string("AbstractSequencer{") + "waitStrategy=" + "..., cursor=..., gatingSequences=...}";
  }

protected:
  int bufferSize_;
  WaitStrategyT waitStrategy_;
  Sequence cursor_;
  std::atomic<std::shared_ptr<std::vector<Sequence*>>> gatingSequences_;
};

} // namespace disruptor

// Inline implementations that need ProcessingSequenceBarrier definition.
#include "ProcessingSequenceBarrier.h"

namespace disruptor {

template <typename WaitStrategyT>
inline std::shared_ptr<ProcessingSequenceBarrier<AbstractSequencer<WaitStrategyT>, WaitStrategyT>>
AbstractSequencer<WaitStrategyT>::newBarrier(Sequence* const* sequencesToTrack, int count) {
  return std::make_shared<ProcessingSequenceBarrier<AbstractSequencer<WaitStrategyT>, WaitStrategyT>>(
      *this, waitStrategy_, cursor_, sequencesToTrack, count);
}

} // namespace disruptor

