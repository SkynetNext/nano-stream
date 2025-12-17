#pragma once
// 1:1 port of com.lmax.disruptor.util.ThreadHints
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/util/ThreadHints.java

#include <thread>

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
#include <immintrin.h> // _mm_pause
#endif

namespace disruptor::util {

class ThreadHints final {
public:
  ThreadHints() = delete;

  // Java: public static void onSpinWait() { Thread.onSpinWait(); }
  static void onSpinWait() {
    // Match Java's Thread.onSpinWait() intent as closely as possible.
    // - On x86/x64, PAUSE is the canonical spin-wait hint.
    // - Fallback to yield() on other platforms.
#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
    _mm_pause();
#else
    std::this_thread::yield();
#endif
  }
};

} // namespace disruptor::util
