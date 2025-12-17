#pragma once
// 1:1 port of com.lmax.disruptor.BatchRewindStrategy
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/BatchRewindStrategy.java

namespace disruptor {

class RewindableException;
enum class RewindAction;

class BatchRewindStrategy {
public:
  virtual ~BatchRewindStrategy() = default;
  virtual RewindAction handleRewindException(const RewindableException& e, int attempts) = 0;
};

} // namespace disruptor
