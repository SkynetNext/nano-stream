#pragma once
// Template-friendly Sequencer (no virtual dispatch).
//
// This file no longer defines a base class. In the template implementation,
// a "Sequencer" is any type that provides the methods used by RingBuffer and
// ProcessingSequenceBarrier:
//
// Required API (subset, depends on producer type):
//   int64_t getCursor() const;
//   int getBufferSize() const;
//   bool hasAvailableCapacity(int requiredCapacity);
//   int64_t remainingCapacity();
//   int64_t next();
//   int64_t next(int n);
//   int64_t tryNext();
//   int64_t tryNext(int n);
//   void publish(int64_t sequence);
//   void publish(int64_t lo, int64_t hi);
//   void addGatingSequences(Sequence* const* gatingSequences, int count);
//   bool removeGatingSequence(Sequence& sequence);
//   std::shared_ptr<ProcessingSequenceBarrier<SequencerT, WaitStrategyT>>
//   newBarrier(...); (via concrete type) int64_t getMinimumSequence(); int64_t
//   getHighestPublishedSequence(int64_t nextSequence, int64_t
//   availableSequence);

#include <cstdint>

namespace disruptor {

// Keep a tiny Sequencer type for constant compatibility during migration.
// This is NOT a polymorphic base class anymore.
struct Sequencer {
  static constexpr int64_t INITIAL_CURSOR_VALUE = -1;
};

inline constexpr int64_t SEQUENCER_INITIAL_CURSOR_VALUE =
    Sequencer::INITIAL_CURSOR_VALUE;

} // namespace disruptor
