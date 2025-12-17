#pragma once
// 1:1 port of com.lmax.disruptor.ProcessingSequenceBarrier
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/ProcessingSequenceBarrier.java

#include "AlertException.h"
#include "FixedSequenceGroup.h"
#include "Sequence.h"
#include "SequenceBarrier.h"
#include "Sequencer.h"
#include "TimeoutException.h"
#include "WaitStrategy.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace disruptor {

// Java: final class ProcessingSequenceBarrier implements SequenceBarrier
class ProcessingSequenceBarrier final : public SequenceBarrier {
public:
  ProcessingSequenceBarrier(Sequencer& sequencer,
                            WaitStrategy& waitStrategy,
                            Sequence& cursorSequence,
                            Sequence* const* dependentSequences,
                            int dependentCount)
      : waitStrategy_(&waitStrategy),
        dependentSequence_(nullptr),
        alerted_(false),
        cursorSequence_(&cursorSequence),
        sequencer_(&sequencer),
        fixedGroup_() {
    if (dependentCount == 0) {
      dependentSequence_ = &cursorSequence;
    } else {
      fixedGroup_ = std::make_unique<FixedSequenceGroup>(dependentSequences, dependentCount);
      dependentSequence_ = fixedGroup_.get();
    }
  }

  int64_t waitFor(int64_t sequence) override {
    checkAlert();

    int64_t availableSequence =
        waitStrategy_->waitFor(sequence, *cursorSequence_, *dependentSequence_, *this);

    if (availableSequence < sequence) {
      return availableSequence;
    }

    return sequencer_->getHighestPublishedSequence(sequence, availableSequence);
  }

  int64_t getCursor() const override { return dependentSequence_->get(); }

  bool isAlerted() const override { return alerted_.load(std::memory_order_acquire); }

  void alert() override {
    alerted_.store(true, std::memory_order_release);
    waitStrategy_->signalAllWhenBlocking();
  }

  void clearAlert() override { alerted_.store(false, std::memory_order_release); }

  void checkAlert() override {
    if (isAlerted()) {
      throw AlertException::INSTANCE();
    }
  }

private:
  WaitStrategy* waitStrategy_;
  Sequence* dependentSequence_;
  std::atomic<bool> alerted_;
  Sequence* cursorSequence_;
  Sequencer* sequencer_;

  // Holds FixedSequenceGroup storage when needed.
  std::unique_ptr<FixedSequenceGroup> fixedGroup_;
};

} // namespace disruptor
