#pragma once
// 1:1 port of com.lmax.disruptor.NoOpEventProcessor
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/NoOpEventProcessor.java

#include "EventProcessor.h"
#include "RingBuffer.h"
#include "Sequence.h"
#include "Sequencer.h"

#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <type_traits>

namespace disruptor {

template <typename T, typename RingBufferT>
class NoOpEventProcessor final : public EventProcessor {
public:
  explicit NoOpEventProcessor(RingBufferT& sequencer)
      : sequence_(sequencer), running_(false) {}

  Sequence& getSequence() override { return sequence_; }

  void halt() override { running_.store(false, std::memory_order_release); }

  bool isRunning() override { return running_.load(std::memory_order_acquire); }

  void run() override {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
      throw std::runtime_error("Thread is already running");
    }
  }

private:
  // Java inner SequencerFollowingSequence extends Sequence and wraps RingBuffer.getCursor().
  class SequencerFollowingSequence final : public Sequence {
  public:
    explicit SequencerFollowingSequence(RingBufferT& sequencer)
        : Sequence(SEQUENCER_INITIAL_CURSOR_VALUE), sequencer_(&sequencer) {
      static_assert(!std::is_reference_v<RingBufferT>, "RingBufferT must not be a reference type");
    }

    int64_t get() const noexcept override { return sequencer_->getCursor(); }

  private:
    std::remove_reference_t<RingBufferT>* sequencer_;
  };

  SequencerFollowingSequence sequence_;
  std::atomic<bool> running_;
};

} // namespace disruptor
