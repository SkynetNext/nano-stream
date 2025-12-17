#pragma once
// 1:1 port of com.lmax.disruptor.EventTranslatorThreeArg
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/EventTranslatorThreeArg.java

#include <cstdint>

namespace disruptor {

template <typename T, typename A, typename B, typename C>
class EventTranslatorThreeArg {
public:
  virtual ~EventTranslatorThreeArg() = default;
  virtual void translateTo(T& event, int64_t sequence, A arg0, B arg1, C arg2) = 0;
};

} // namespace disruptor
