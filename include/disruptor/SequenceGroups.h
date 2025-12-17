#pragma once
// 1:1 port of com.lmax.disruptor.SequenceGroups
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/SequenceGroups.java

#include "Cursored.h"
#include "Sequence.h"

#include <atomic>
#include <memory>
#include <vector>

namespace disruptor {

class SequenceGroups final {
public:
  SequenceGroups() = delete;

  // Java uses AtomicReferenceFieldUpdater<T, Sequence[]> on a volatile field.
  // C++ port uses atomic shared_ptr to an immutable vector snapshot.
  template <typename Holder>
  static void addSequences(Holder& holder,
                           std::atomic<std::shared_ptr<std::vector<Sequence*>>>& updater,
                           const Cursored& cursor,
                           Sequence* const* sequencesToAdd,
                           int count) {
    int64_t cursorSequence = 0;
    std::shared_ptr<std::vector<Sequence*>> currentSequences;
    std::shared_ptr<std::vector<Sequence*>> updatedSequences;

    do {
      currentSequences = updater.load(std::memory_order_acquire);
      if (!currentSequences) {
        currentSequences = std::make_shared<std::vector<Sequence*>>();
      }
      updatedSequences = std::make_shared<std::vector<Sequence*>>(*currentSequences);
      updatedSequences->reserve(updatedSequences->size() + static_cast<size_t>(count));

      cursorSequence = cursor.getCursor();
      for (int i = 0; i < count; ++i) {
        auto* seq = sequencesToAdd[i];
        if (seq) {
          seq->set(cursorSequence);
        }
        updatedSequences->push_back(seq);
      }
    } while (!updater.compare_exchange_weak(currentSequences,
                                           updatedSequences,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire));

    cursorSequence = cursor.getCursor();
    for (int i = 0; i < count; ++i) {
      auto* seq = sequencesToAdd[i];
      if (seq) {
        seq->set(cursorSequence);
      }
    }
  }

  template <typename Holder>
  static bool removeSequence(Holder& holder,
                             std::atomic<std::shared_ptr<std::vector<Sequence*>>>& updater,
                             Sequence& sequence) {
    (void)holder;
    int numToRemove = 0;
    std::shared_ptr<std::vector<Sequence*>> oldSequences;
    std::shared_ptr<std::vector<Sequence*>> newSequences;

    do {
      oldSequences = updater.load(std::memory_order_acquire);
      if (!oldSequences) {
        break;
      }

      numToRemove = 0;
      for (auto* s : *oldSequences) {
        if (s == &sequence) {
          ++numToRemove;
        }
      }

      if (numToRemove == 0) {
        break;
      }

      newSequences = std::make_shared<std::vector<Sequence*>>();
      newSequences->reserve(oldSequences->size() - static_cast<size_t>(numToRemove));
      for (auto* s : *oldSequences) {
        if (s != &sequence) {
          newSequences->push_back(s);
        }
      }
    } while (!updater.compare_exchange_weak(oldSequences,
                                           newSequences,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire));

    return numToRemove != 0;
  }
};

} // namespace disruptor
