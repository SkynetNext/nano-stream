#pragma once
// 1:1 port of com.lmax.disruptor.EventTranslator
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/EventTranslator.java

#include <cstdint>

namespace disruptor {

template <typename T>
class EventTranslator {
public:
  virtual ~EventTranslator() = default;
  virtual void translateTo(T& event, int64_t sequence) = 0;
};

} // namespace disruptor
