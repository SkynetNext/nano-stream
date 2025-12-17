#pragma once
// Test stub equivalent to com.lmax.disruptor.dsl.stubs.SleepingEventHandler

#include "tests/disruptor/support/TestEvent.h"

#include "disruptor/EventHandler.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace disruptor::dsl::stubs {

// In Java this handler sleeps/yields to simulate work.
class SleepingEventHandler final : public ::disruptor::EventHandler<disruptor::support::TestEvent> {
public:
  void onEvent(disruptor::support::TestEvent& /*event*/, int64_t /*sequence*/, bool /*endOfBatch*/) override {
    ++eventsSeen;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  std::atomic<int64_t> eventsSeen{0};
};

} // namespace disruptor::dsl::stubs


