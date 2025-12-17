#pragma once
// 1:1 port skeleton of com.lmax.disruptor.BusySpinWaitStrategy

#include "AlertException.h"
#include "Sequence.h"
#include "WaitStrategy.h"

#if defined(_MSC_VER)
#include <immintrin.h>
#endif

namespace disruptor {

class BusySpinWaitStrategy final {
public:
  static constexpr bool kIsBlockingStrategy = false;

  template <typename Barrier>
  int64_t waitFor(int64_t sequence, const Sequence & /*cursor*/,
                  const Sequence &dependentSequence, Barrier &barrier) {
    int64_t available;
    while ((available = dependentSequence.get()) < sequence) {
      barrier.checkAlert();
#if defined(_MSC_VER)
      _mm_pause();
#elif defined(__i386__) || defined(__x86_64__)
      __builtin_ia32_pause();
#endif
    }
    return available;
  }

  void signalAllWhenBlocking() {}
};

} // namespace disruptor
