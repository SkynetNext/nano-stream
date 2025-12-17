#pragma once
// 1:1 port of com.lmax.disruptor.Sequencer
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/Sequencer.java

#include "Cursored.h"
#include "Sequenced.h"

#include <cstdint>

namespace disruptor {

class Sequence;
class SequenceBarrier;

template <typename T> class DataProvider;
template <typename T> class EventPoller;

class Sequencer : public Cursored, public Sequenced {
public:
  static constexpr int64_t INITIAL_CURSOR_VALUE = -1;

  ~Sequencer() override = default;

  virtual void claim(int64_t sequence) = 0;
  virtual bool isAvailable(int64_t sequence) = 0;

  virtual void addGatingSequences(Sequence* const* gatingSequences, int count) = 0;
  virtual bool removeGatingSequence(Sequence& sequence) = 0;

  virtual std::shared_ptr<SequenceBarrier> newBarrier(Sequence* const* sequencesToTrack, int count) = 0;

  virtual int64_t getMinimumSequence() = 0;
  virtual int64_t getHighestPublishedSequence(int64_t nextSequence, int64_t availableSequence) = 0;

  template <typename T>
  std::shared_ptr<EventPoller<T>> newPoller(DataProvider<T>& /*provider*/, Sequence* const* /*gatingSequences*/, int /*count*/) {
    // Implemented in AbstractSequencer in Java; C++ port follows by providing a concrete override there.
    return nullptr;
  }
};

} // namespace disruptor
