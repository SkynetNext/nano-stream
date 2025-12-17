#pragma once
// 1:1 port of com.lmax.disruptor.EventTranslatorVararg
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/EventTranslatorVararg.java

#include <cstdint>
#include <vector>

namespace disruptor {

template <typename T>
class EventTranslatorVararg {
public:
  virtual ~EventTranslatorVararg() = default;
  // Java uses Object... args. In C++ we model as an array of opaque pointers.
  virtual void translateTo(T& event, int64_t sequence, const std::vector<void*>& args) = 0;
};

} // namespace disruptor
