#pragma once
// 1:1 port of com.lmax.disruptor.RewindHandler
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/RewindHandler.java

#include <cstdint>

namespace disruptor {

class RewindableException;

class RewindHandler {
public:
  virtual ~RewindHandler() = default;
  virtual int64_t attemptRewindGetNextSequence(const RewindableException& e, int64_t startOfBatchSequence) = 0;
};

} // namespace disruptor
