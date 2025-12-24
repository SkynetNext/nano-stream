#pragma once
// 1:1 port of com.lmax.disruptor.dsl.stubs.ExceptionThrowingEventHandler
// Source: reference/disruptor/src/test/java/com/lmax/disruptor/dsl/stubs/ExceptionThrowingEventHandler.java

#include "disruptor/EventHandler.h"
#include "tests/disruptor/support/TestEvent.h"

#include <stdexcept>

namespace disruptor::dsl::stubs {

class ExceptionThrowingEventHandler final
    : public disruptor::EventHandler<disruptor::support::TestEvent> {
public:
  explicit ExceptionThrowingEventHandler(std::runtime_error* testException)
      : testException_(testException) {}

  void onEvent(disruptor::support::TestEvent& /*entry*/, int64_t /*sequence*/,
               bool /*endOfBatch*/) override {
    if (testException_) {
      throw *testException_;
    }
  }

private:
  std::runtime_error* testException_;
};

} // namespace disruptor::dsl::stubs

