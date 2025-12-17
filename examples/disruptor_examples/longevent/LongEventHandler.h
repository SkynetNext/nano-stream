#pragma once
// 1:1 port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/longevent/LongEventHandler.java

#include "disruptor/EventHandler.h"
#include "LongEvent.h"

#include <cstdint>
#include <iostream>

namespace disruptor_examples::longevent {

class LongEventHandler final : public disruptor::EventHandler<LongEvent> {
public:
  void onEvent(LongEvent& event, int64_t /*sequence*/, bool /*endOfBatch*/) override {
    std::cout << "Event: " << event.toString() << "\n";
  }
};

} // namespace disruptor_examples::longevent


