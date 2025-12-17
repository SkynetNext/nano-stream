#pragma once
// Minimal stand-in for java.nio.ByteBuffer used by Disruptor Java examples.

#include <cstdint>

namespace disruptor_examples::longevent {

class ByteBuffer {
public:
  static ByteBuffer allocate(int /*bytes*/) { return ByteBuffer(); }
  void putLong(int /*index*/, int64_t v) { value_ = v; }
  int64_t getLong(int /*index*/) const { return value_; }

private:
  int64_t value_{0};
};

} // namespace disruptor_examples::longevent


