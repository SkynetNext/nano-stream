#pragma once

// Prevent Windows min/max macros from interfering with std::min/max
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "sequence.h"
#include "wait_strategy.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace disruptor {

/**
 * Exception thrown when a barrier is alerted.
 */
class AlertException : public std::exception {
public:
  const char *what() const noexcept override { return "AlertException"; }
};

/**
 * Exception thrown when waiting for a sequence times out.
 */
class TimeoutException : public std::exception {
public:
  const char *what() const noexcept override { return "TimeoutException"; }
};

/**
 * Coordination barrier for tracking the cursor for publishers and sequence of
 * dependent EventProcessors for processing a data structure.
 */
class SequenceBarrier {
public:
  virtual ~SequenceBarrier() = default;

  /**
   * Wait for the given sequence to be available for consumption.
   *
   * @param sequence to wait for
   * @return the sequence up to which is available
   * @throws AlertException if a status change has occurred for the Disruptor
   * @throws TimeoutException if a timeout occurs while waiting for the supplied
   * sequence.
   */
  virtual int64_t wait_for(int64_t sequence) = 0;

  /**
   * Get the current cursor value that can be read.
   *
   * @return value of the cursor for entries that have been published.
   */
  virtual int64_t get_cursor() const = 0;

  /**
   * The current alert status for the barrier.
   *
   * @return true if in alert otherwise false.
   */
  virtual bool is_alerted() const = 0;

  /**
   * Alert the EventProcessors of a status change and stay in this status until
   * cleared.
   */
  virtual void alert() = 0;

  /**
   * Clear the current alert status.
   */
  virtual void clear_alert() = 0;

  /**
   * Check if an alert has been raised and throw an AlertException if it has.
   *
   * @throws AlertException if alert has been raised.
   */
  virtual void check_alert() = 0;
};

/**
 * Fixed sequence group for tracking multiple dependent sequences.
 */
class FixedSequenceGroup {
private:
  std::vector<std::reference_wrapper<const Sequence>> sequences_;

public:
  explicit FixedSequenceGroup(
      const std::vector<std::reference_wrapper<const Sequence>> &sequences)
      : sequences_(sequences) {}

  /**
   * Get the minimum sequence from all sequences in the group.
   */
  int64_t get() const {
    if (sequences_.empty()) {
      return Sequence::INITIAL_VALUE;
    }

    int64_t min_seq = sequences_[0].get().get();
    for (size_t i = 1; i < sequences_.size(); ++i) {
      int64_t seq = sequences_[i].get().get();
      if (seq < min_seq) {
        min_seq = seq;
      }
    }
    return min_seq;
  }
};

/**
 * Processing sequence barrier implementation.
 * Handed out for gating EventProcessors on a cursor sequence and optional
 * dependent EventProcessors.
 */
class ProcessingSequenceBarrier : public SequenceBarrier {
private:
  std::unique_ptr<WaitStrategy> wait_strategy_;
  std::unique_ptr<FixedSequenceGroup> dependent_sequence_;
  std::atomic<bool> alerted_{false};
  const Sequence &cursor_sequence_;

public:
  ProcessingSequenceBarrier(
      std::unique_ptr<WaitStrategy> wait_strategy,
      const Sequence &cursor_sequence,
      const std::vector<std::reference_wrapper<const Sequence>>
          &dependent_sequences)
      : wait_strategy_(std::move(wait_strategy)),
        cursor_sequence_(cursor_sequence) {
    if (dependent_sequences.empty()) {
      dependent_sequence_ = std::make_unique<FixedSequenceGroup>(
          std::vector<std::reference_wrapper<const Sequence>>{cursor_sequence});
    } else {
      dependent_sequence_ =
          std::make_unique<FixedSequenceGroup>(dependent_sequences);
    }
  }

  int64_t wait_for(int64_t sequence) override {
    check_alert();

    int64_t available_sequence = wait_strategy_->wait_for(
        sequence, cursor_sequence_, dependent_sequence_->get());

    if (available_sequence < sequence) {
      return available_sequence;
    }

    // Return the highest published sequence up to the requested sequence
    return (available_sequence < sequence) ? available_sequence : sequence;
  }

  int64_t get_cursor() const override { return dependent_sequence_->get(); }

  bool is_alerted() const override { return alerted_.load(); }

  void alert() override {
    alerted_.store(true);
    wait_strategy_->signal_all_when_blocking();
  }

  void clear_alert() override { alerted_.store(false); }

  void check_alert() override {
    if (alerted_.load()) {
      throw AlertException();
    }
  }
};

} // namespace disruptor
