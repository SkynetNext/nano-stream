#pragma once
// 1:1 port of com.lmax.disruptor.EventTranslatorTwoArg
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/EventTranslatorTwoArg.java

#include <cstdint>

namespace disruptor {

template <typename T, typename A, typename B>
class EventTranslatorTwoArg {
public:
  virtual ~EventTranslatorTwoArg() = default;
  virtual void translateTo(T& event, int64_t sequence, A arg0, B arg1) = 0;
};

} // namespace disruptor
