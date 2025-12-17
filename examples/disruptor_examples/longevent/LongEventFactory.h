#pragma once
// 1:1 port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/longevent/LongEventFactory.java

#include "disruptor/EventFactory.h"
#include "LongEvent.h"

namespace disruptor_examples::longevent {

class LongEventFactory final : public disruptor::EventFactory<LongEvent> {
public:
  LongEvent newInstance() override { return LongEvent(); }
};

} // namespace disruptor_examples::longevent


