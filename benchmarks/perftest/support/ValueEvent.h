#pragma once
// 1:1 port of com.lmax.disruptor.support.ValueEvent
// Source: reference/disruptor/src/perftest/java/com/lmax/disruptor/support/ValueEvent.java

#include "disruptor/EventFactory.h"

namespace nano_stream::bench::perftest::support {

class ValueEvent {
public:
  ValueEvent() : value_(0) {}
  explicit ValueEvent(int64_t value) : value_(value) {}

  int64_t getValue() const { return value_; }
  void setValue(int64_t value) { value_ = value; }

  static std::shared_ptr<disruptor::EventFactory<ValueEvent>> EVENT_FACTORY;

private:
  int64_t value_;
};

// Factory implementation
class ValueEventFactory : public disruptor::EventFactory<ValueEvent> {
public:
  ValueEvent newInstance() override { return ValueEvent(); }
};

// Static factory instance
inline std::shared_ptr<disruptor::EventFactory<ValueEvent>>
    ValueEvent::EVENT_FACTORY = std::make_shared<ValueEventFactory>();

} // namespace nano_stream::bench::perftest::support

