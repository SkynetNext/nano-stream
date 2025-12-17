#pragma once
// 1:1 port of com.lmax.disruptor.AbstractSequencer
// Source:
// reference/disruptor/src/main/java/com/lmax/disruptor/AbstractSequencer.java

#include "Cursored.h"
#include "Sequence.h"
#include "Sequencer.h"
#include "WaitStrategy.h"

#include "EventPoller.h"
#include "SequenceGroups.h"
#include "util/Util.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace disruptor {

template <typename T> class DataProvider;
template <typename T, typename SequencerT> class EventPoller;

// Java uses AtomicReferenceFieldUpdater over a volatile Sequence[] for
// gatingSequences. C++ port uses an atomic shared_ptr snapshot updated via CAS
// (see SequenceGroups).
//
// Template version: WaitStrategy is stored by value and statically dispatched.
template <typename WaitStrategyT> class AbstractSequencer : public Cursored {
public:
  explicit AbstractSequencer(int bufferSize, WaitStrategyT &waitStrategy)
      : bufferSize_(bufferSize), waitStrategy_(&waitStrategy),
        cursor_(Sequencer::INITIAL_CURSOR_VALUE),
        gatingSequences_(std::make_shared<std::vector<Sequence *>>()) {
    if (bufferSize < 1) {
      throw std::invalid_argument("bufferSize must not be less than 1");
    }
    if ((bufferSize & (bufferSize - 1)) != 0) {
      throw std::invalid_argument("bufferSize must be a power of 2");
    }
  }

  int64_t getCursor() const override { return cursor_.get(); }
  int getBufferSize() const { return bufferSize_; }

  void addGatingSequences(Sequence *const *gatingSequences, int count) {
    SequenceGroups::addSequences(*this, gatingSequences_, *this,
                                 gatingSequences, count);
  }

  bool removeGatingSequence(Sequence &sequence) {
    return SequenceGroups::removeSequence(*this, gatingSequences_, sequence);
  }

  int64_t getMinimumSequence() {
    auto snap = gatingSequences_.load(std::memory_order_acquire);
    if (!snap) {
      return cursor_.get();
    }
    return disruptor::util::Util::getMinimumSequence(*snap, cursor_.get());
  }

  // NOTE: newBarrier is implemented on concrete sequencers so the barrier is
  // bound to the concrete Sequencer type (which provides
  // getHighestPublishedSequence()).

  std::string toString() const {
    return std::string("AbstractSequencer{") +
           "waitStrategy=" + "..., cursor=..., gatingSequences=...}";
  }

protected:
  int bufferSize_;
  WaitStrategyT *waitStrategy_;
  Sequence cursor_;
  std::atomic<std::shared_ptr<std::vector<Sequence *>>> gatingSequences_;
};

} // namespace disruptor
