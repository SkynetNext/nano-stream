#pragma once
// 1:1 port of com.lmax.disruptor.support.DummyEventProcessor

#include "disruptor/EventProcessor.h"
#include "disruptor/Sequence.h"
#include "disruptor/SingleProducerSequencer.h"

#include <atomic>
#include <stdexcept>

namespace disruptor::support {

class DummyEventProcessor final : public ::disruptor::EventProcessor {
public:
  DummyEventProcessor() : sequence_(::disruptor::Sequencer::INITIAL_CURSOR_VALUE) {}
  explicit DummyEventProcessor(int64_t initialSequence) : sequence_(initialSequence) {}

  void setSequence(int64_t sequence) { sequence_.set(sequence); }

  ::disruptor::Sequence& getSequence() override { return sequence_; }

  void halt() override { running_.store(false, std::memory_order_release); }

  bool isRunning() override { return running_.load(std::memory_order_acquire); }

  void run() override {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
      throw std::runtime_error("Already running");
    }
  }

private:
  ::disruptor::Sequence sequence_;
  std::atomic<bool> running_{false};
};

} // namespace disruptor::support


