#pragma once
// 1:1 port of com.lmax.disruptor.RewindAction
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/RewindAction.java

namespace disruptor {

enum class RewindAction {
  REWIND,
  THROW,
};

} // namespace disruptor
