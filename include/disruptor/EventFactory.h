#pragma once
// 1:1 port skeleton of com.lmax.disruptor.EventFactory

namespace disruptor {
template <typename E>
class EventFactory {
public:
  virtual ~EventFactory() = default;
  virtual E newInstance() = 0;
};
} // namespace disruptor


