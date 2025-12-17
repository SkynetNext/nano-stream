#pragma once
// 1:1 port skeleton of com.lmax.disruptor.Cursored

#include <cstdint>

namespace disruptor {

class Cursored {
public:
  virtual ~Cursored() = default;
  virtual int64_t getCursor() const = 0;
};

} // namespace disruptor


