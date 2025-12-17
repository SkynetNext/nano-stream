#pragma once
// 1:1 port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/longevent/LongEvent.java

#include <cstdint>
#include <string>

namespace disruptor_examples::longevent {

class LongEvent {
public:
  void set(int64_t value) { value_ = value; }

  std::string toString() const {
    return std::string("LongEvent{") + "value=" + std::to_string(value_) + "}";
  }

private:
  int64_t value_{0};
};

} // namespace disruptor_examples::longevent


