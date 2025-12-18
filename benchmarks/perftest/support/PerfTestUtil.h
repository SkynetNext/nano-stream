#pragma once
// 1:1 port of com.lmax.disruptor.support.PerfTestUtil
// Source: reference/disruptor/src/perftest/java/com/lmax/disruptor/support/PerfTestUtil.java

#include <cstdint>
#include <stdexcept>

namespace nano_stream::bench::perftest::support {

inline int64_t accumulatedAddition(int64_t iterations) {
  int64_t temp = 0;
  for (int64_t i = 0; i < iterations; i++) {
    temp += i;
  }
  return temp;
}

inline void failIf(int64_t a, int64_t b) {
  if (a == b) {
    throw std::runtime_error("failIf: values are equal");
  }
}

inline void failIfNot(int64_t a, int64_t b) {
  if (a != b) {
    throw std::runtime_error("failIfNot: values are not equal");
  }
}

} // namespace nano_stream::bench::perftest::support

