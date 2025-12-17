#pragma once
// 1:1 port of com.lmax.disruptor.util.ThreadHints
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/util/ThreadHints.java

#include <thread>

namespace disruptor::util {

class ThreadHints final {
public:
  ThreadHints() = delete;

  // Java: public static void onSpinWait() { Thread.onSpinWait(); }
  static void onSpinWait() {
    // C++20: std::this_thread::yield is the closest portable hint.
    // Some compilers provide intrinsics; we keep this portable here.
    std::this_thread::yield();
  }
};

} // namespace disruptor::util
