#pragma once
// 1:1 port of com.lmax.disruptor.EventTranslatorOneArg
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/EventTranslatorOneArg.java

#include <cstdint>

namespace disruptor {

template <typename T, typename A>
class EventTranslatorOneArg {
public:
  virtual ~EventTranslatorOneArg() = default;
  virtual void translateTo(T& event, int64_t sequence, A arg0) = 0;
};

} // namespace disruptor
