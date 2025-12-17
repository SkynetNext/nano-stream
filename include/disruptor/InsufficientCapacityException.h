#pragma once
// 1:1 port of com.lmax.disruptor.InsufficientCapacityException
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/InsufficientCapacityException.java

#include <stdexcept>

namespace disruptor {

class InsufficientCapacityException final : public std::runtime_error {
public:
  static const InsufficientCapacityException& INSTANCE() {
    static InsufficientCapacityException instance;
    return instance;
  }

private:
  InsufficientCapacityException() : std::runtime_error("InsufficientCapacityException") {}
};

} // namespace disruptor


