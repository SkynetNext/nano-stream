#pragma once
// 1:1 port of com.lmax.disruptor.SequenceGroup
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/SequenceGroup.java

#include "Cursored.h"
#include "Sequence.h"
#include "SequenceGroups.h"
#include "util/Util.h"

#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace disruptor {

class SequenceGroup final : public Sequence, public Cursored {
public:
  SequenceGroup() : Sequence(-1), sequences_(std::make_shared<std::vector<Sequence*>>()) {}

  int64_t get() const noexcept override {
    auto snap = sequences_.load(std::memory_order_acquire);
    if (!snap) {
      // Guard against Windows macro max()
      return (std::numeric_limits<int64_t>::max)();
    }
    return disruptor::util::Util::getMinimumSequence(*snap);
  }

  int64_t getCursor() const override { return get(); }

  void set(int64_t value) noexcept override {
    auto snap = sequences_.load(std::memory_order_acquire);
    if (!snap) return;
    for (auto* s : *snap) {
      if (s) s->set(value);
    }
  }

  void add(Sequence& sequence) {
    Sequence* ptr = &sequence;
    Sequence* const arr[1] = {ptr};
    // This mirrors Java CAS update of the array.
    SequenceGroups::addSequences(*this, sequences_, *this, arr, 1);
  }

  bool remove(Sequence& sequence) {
    return SequenceGroups::removeSequence(*this, sequences_, sequence);
  }

  int size() const {
    auto snap = sequences_.load(std::memory_order_acquire);
    return snap ? static_cast<int>(snap->size()) : 0;
  }

  void addWhileRunning(const Cursored& cursored, Sequence& sequence) {
    Sequence* ptr = &sequence;
    Sequence* const arr[1] = {ptr};
    SequenceGroups::addSequences(*this, sequences_, cursored, arr, 1);
  }

private:
  std::atomic<std::shared_ptr<std::vector<Sequence*>>> sequences_;
};

} // namespace disruptor
