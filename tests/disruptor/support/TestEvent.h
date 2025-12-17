#pragma once
// 1:1 port of com.lmax.disruptor.support.TestEvent (test helper)

#include "disruptor/EventFactory.h"

#include <string>

namespace disruptor::support {

struct TestEvent {
  std::string toString() const { return "Test Event"; }

  struct Factory final : public ::disruptor::EventFactory<TestEvent> {
    TestEvent newInstance() override { return TestEvent(); }
  };

  static inline std::shared_ptr<::disruptor::EventFactory<TestEvent>> EVENT_FACTORY = std::make_shared<Factory>();
};

} // namespace disruptor::support


