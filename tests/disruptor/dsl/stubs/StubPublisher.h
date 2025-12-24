#pragma once
// 1:1 port of com.lmax.disruptor.dsl.stubs.StubPublisher
// Source: reference/disruptor/src/test/java/com/lmax/disruptor/dsl/stubs/StubPublisher.java

#include "disruptor/RingBuffer.h"
#include "disruptor/InsufficientCapacityException.h"
#include "tests/disruptor/support/TestEvent.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace disruptor::dsl::stubs {

template <typename SequencerT>
class StubPublisher {
public:
  explicit StubPublisher(disruptor::RingBuffer<disruptor::support::TestEvent, SequencerT>& ringBuffer)
      : running_(true), publicationCount_(0), ringBuffer_(&ringBuffer) {}

  void run() {
    while (running_.load()) {
      try {
        // Use tryNext() instead of next() to allow checking running_ flag
        // when buffer is full. This matches Java's behavior where LockSupport.parkNanos(1L)
        // in next() allows periodic wake-up, but on Windows std::this_thread::yield()
        // may not wake up reliably.
        int64_t sequence = ringBuffer_->tryNext();
        ringBuffer_->publish(sequence);
        publicationCount_.fetch_add(1);
      } catch (const disruptor::InsufficientCapacityException&) {
        // Buffer is full, wait a bit and check running_ flag
        // This allows halt() to interrupt the blocking wait
        if (!running_.load()) {
          break;
        }
        // Use sleep_for(1ns) to match Java's LockSupport.parkNanos(1L) behavior
        std::this_thread::sleep_for(std::chrono::nanoseconds(1));
      }
    }
  }

  int getPublicationCount() const { return publicationCount_.load(); }

  void halt() { running_.store(false); }

private:
  std::atomic<bool> running_;
  std::atomic<int> publicationCount_;
  disruptor::RingBuffer<disruptor::support::TestEvent, SequencerT>* ringBuffer_;
};

} // namespace disruptor::dsl::stubs

