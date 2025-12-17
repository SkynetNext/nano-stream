#pragma once
// 1:1 port of com.lmax.disruptor.EventHandlerIdentity
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/EventHandlerIdentity.java

namespace disruptor {

class EventHandlerIdentity {
public:
  virtual ~EventHandlerIdentity() = default;
};

} // namespace disruptor
