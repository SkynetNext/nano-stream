#pragma once
// 1:1 port of com.lmax.disruptor.dsl.stubs.StubPublisher
// Source: reference/disruptor/src/test/java/com/lmax/disruptor/dsl/stubs/StubPublisher.java

#include "disruptor/RingBuffer.h"
#include "tests/disruptor/support/TestEvent.h"

#include <atomic>
#include <thread>

namespace disruptor::dsl::stubs {

template <typename SequencerT>
class StubPublisher {
public:
  explicit StubPublisher(disruptor::RingBuffer<disruptor::support::TestEvent, SequencerT>& ringBuffer)
      : running_(true), publicationCount_(0), ringBuffer_(&ringBuffer) {}

  void run() {
    while (running_.load()) {
      int64_t sequence = ringBuffer_->next();
      ringBuffer_->publish(sequence);
      publicationCount_.fetch_add(1);
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

