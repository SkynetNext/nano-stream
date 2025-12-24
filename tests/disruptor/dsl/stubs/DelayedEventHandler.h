#pragma once
// 1:1 port of com.lmax.disruptor.dsl.stubs.DelayedEventHandler
// Source: reference/disruptor/src/test/java/com/lmax/disruptor/dsl/stubs/DelayedEventHandler.java

#include "disruptor/EventHandler.h"
#include "tests/disruptor/support/TestEvent.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace disruptor::dsl::stubs {

class DelayedEventHandler final
    : public disruptor::EventHandler<disruptor::support::TestEvent> {
public:
  DelayedEventHandler() : readyToProcessEvent_(false), stopped_(false) {}

  void onEvent(disruptor::support::TestEvent& /*entry*/, int64_t /*sequence*/,
               bool /*endOfBatch*/) override {
    waitForAndSetFlag(false);
  }

  void processEvent() { waitForAndSetFlag(true); }

  void stopWaiting() {
    stopped_.store(true);
  }

  void onStart() override {
    std::lock_guard<std::mutex> lock(mutex_);
    started_ = true;
    cv_.notify_all();
  }

  void onShutdown() override {}

  void awaitStart() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return started_; });
  }

private:
  void waitForAndSetFlag(bool newValue) {
    bool expected = !newValue;
    while (!stopped_.load() && 
           !readyToProcessEvent_.compare_exchange_weak(expected, newValue)) {
      expected = !newValue;
      std::this_thread::yield();
    }
  }

  std::atomic<bool> readyToProcessEvent_;
  std::atomic<bool> stopped_;
  bool started_ = false;
  std::mutex mutex_;
  std::condition_variable cv_;
};

} // namespace disruptor::dsl::stubs

