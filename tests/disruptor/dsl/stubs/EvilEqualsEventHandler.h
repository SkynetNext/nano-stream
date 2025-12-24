#pragma once
// 1:1 port of com.lmax.disruptor.dsl.stubs.EvilEqualsEventHandler
// Source: reference/disruptor/src/test/java/com/lmax/disruptor/dsl/stubs/EvilEqualsEventHandler.java

#include "disruptor/EventHandler.h"
#include "tests/disruptor/support/TestEvent.h"

namespace disruptor::dsl::stubs {

// This handler has an evil equals() that always returns true
// to test that Disruptor tracks handlers by identity, not equality
class EvilEqualsEventHandler final
    : public disruptor::EventHandler<disruptor::support::TestEvent> {
public:
  void onEvent(disruptor::support::TestEvent& /*entry*/, int64_t /*sequence*/,
               bool /*endOfBatch*/) override {}

  // Evil equals that always returns true
  bool operator==(const EvilEqualsEventHandler& /*other*/) const { return true; }
};

} // namespace disruptor::dsl::stubs

