#pragma once
// 1:1 port skeleton of com.lmax.disruptor.DataProvider

#include <cstdint>

namespace disruptor {
template <typename T>
class DataProvider {
public:
  virtual ~DataProvider() = default;
  virtual T& get(int64_t sequence) = 0;
};
} // namespace disruptor


