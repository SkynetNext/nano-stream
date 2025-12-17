#pragma once
// 1:1 port of com.lmax.disruptor.FixedSequenceGroup
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/FixedSequenceGroup.java

#include "Sequence.h"
#include "util/Util.h"

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace disruptor {

class FixedSequenceGroup final : public Sequence {
public:
  // Java copies the array; we store pointers and treat them as identity.
  FixedSequenceGroup(Sequence* const* sequences, int count)
      : sequences_() {
    sequences_.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
      sequences_.push_back(sequences[i]);
    }
  }

  int64_t get() const noexcept override {
    return disruptor::util::Util::getMinimumSequence(sequences_);
  }

  std::string toString() const { return "FixedSequenceGroup{...}"; }

  void set(int64_t /*value*/) override {
    throw std::runtime_error("UnsupportedOperationException");
  }

  bool compareAndSet(int64_t /*expectedValue*/, int64_t /*newValue*/) override {
    throw std::runtime_error("UnsupportedOperationException");
  }

  int64_t incrementAndGet() override {
    throw std::runtime_error("UnsupportedOperationException");
  }

  int64_t addAndGet(int64_t /*increment*/) override {
    throw std::runtime_error("UnsupportedOperationException");
  }

private:
  std::vector<Sequence*> sequences_;
};

} // namespace disruptor
