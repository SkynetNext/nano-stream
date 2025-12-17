#pragma once
// 1:1 port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/support/LongEvent.java

#include "disruptor/EventFactory.h"

#include <cstdint>
#include <memory>

namespace disruptor_examples::support {

class LongEvent {
public:
  void set(int64_t value) { value_ = value; }
  int64_t get() const { return value_; }

  struct Factory final : public disruptor::EventFactory<LongEvent> {
    LongEvent newInstance() override { return LongEvent(); }
  };

  static inline std::shared_ptr<disruptor::EventFactory<LongEvent>> FACTORY = std::make_shared<Factory>();

private:
  int64_t value_{0};
};

} // namespace disruptor_examples::support


