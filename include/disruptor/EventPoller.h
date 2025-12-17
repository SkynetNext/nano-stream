#pragma once
// 1:1 port of com.lmax.disruptor.EventPoller
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/EventPoller.java

#include "DataProvider.h"
#include "FixedSequenceGroup.h"
#include "Sequence.h"
#include "Sequencer.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace disruptor {

template <typename T>
class EventPoller {
public:
  class Handler {
  public:
    virtual ~Handler() = default;
    virtual bool onEvent(T& event, int64_t sequence, bool endOfBatch) = 0;
  };

  enum class PollState {
    PROCESSING,
    GATING,
    IDLE
  };

  EventPoller(DataProvider<T>& dataProvider,
              Sequencer& sequencer,
              std::shared_ptr<Sequence> sequence,
              Sequence& gatingSequence)
      : dataProvider_(&dataProvider),
        sequencer_(&sequencer),
        ownedSequence_(std::move(sequence)),
        sequence_(ownedSequence_.get()),
        gatingSequence_(&gatingSequence),
        fixedGroup_(nullptr) {}

  PollState poll(Handler& eventHandler) {
    const int64_t currentSequence = sequence_->get();
    int64_t nextSequence = currentSequence + 1;
    const int64_t availableSequence =
        sequencer_->getHighestPublishedSequence(nextSequence, gatingSequence_->get());

    if (nextSequence <= availableSequence) {
      bool processNextEvent;
      int64_t processedSequence = currentSequence;
      try {
        do {
          T& event = dataProvider_->get(nextSequence);
          processNextEvent = eventHandler.onEvent(event, nextSequence, nextSequence == availableSequence);
          processedSequence = nextSequence;
          ++nextSequence;
        } while (nextSequence <= availableSequence && processNextEvent);
      } catch (...) {
        sequence_->set(processedSequence);
        throw;
      }
      sequence_->set(processedSequence);
      return PollState::PROCESSING;
    } else if (sequencer_->getCursor() >= nextSequence) {
      return PollState::GATING;
    } else {
      return PollState::IDLE;
    }
  }

  static std::shared_ptr<EventPoller<T>> newInstance(DataProvider<T>& dataProvider,
                                                     Sequencer& sequencer,
                                                     std::shared_ptr<Sequence> sequence,
                                                     Sequence& cursorSequence,
                                                     Sequence* const* gatingSequences,
                                                     int gatingCount) {
    if (gatingCount == 0) {
      return std::make_shared<EventPoller<T>>(dataProvider, sequencer, std::move(sequence), cursorSequence);
    }
    if (gatingCount == 1) {
      return std::make_shared<EventPoller<T>>(dataProvider, sequencer, std::move(sequence), *gatingSequences[0]);
    }
    auto poller = std::make_shared<EventPoller<T>>(dataProvider, sequencer, std::move(sequence), cursorSequence);
    poller->fixedGroup_ = std::make_unique<FixedSequenceGroup>(gatingSequences, gatingCount);
    poller->gatingSequence_ = poller->fixedGroup_.get();
    return poller;
  }

  Sequence& getSequence() { return *sequence_; }

private:
  DataProvider<T>* dataProvider_;
  Sequencer* sequencer_;
  std::shared_ptr<Sequence> ownedSequence_;
  Sequence* sequence_;
  Sequence* gatingSequence_;
  std::unique_ptr<FixedSequenceGroup> fixedGroup_;
};

} // namespace disruptor
