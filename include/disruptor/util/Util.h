#pragma once
// 1:1 port of com.lmax.disruptor.util.Util
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/util/Util.java

#include "../EventProcessor.h"
#include "../Sequence.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace disruptor::util {

class Util final {
public:
  Util() = delete;

  static constexpr int ONE_MILLISECOND_IN_NANOSECONDS = 1'000'000;

  static int ceilingNextPowerOfTwo(int x) {
    // Java: 1 << (Integer.SIZE - Integer.numberOfLeadingZeros(x - 1))
    if (x <= 0) {
      throw std::invalid_argument("x must be a positive number");
    }
    uint32_t v = static_cast<uint32_t>(x - 1);
    int leading = 0;
#if defined(_MSC_VER)
    unsigned long idx = 0;
    if (_BitScanReverse(&idx, v) != 0) {
      leading = 31 - static_cast<int>(idx);
    } else {
      leading = 32;
    }
#else
    leading = __builtin_clz(v);
#endif
    return 1 << (32 - leading);
  }

  static int64_t getMinimumSequence(const std::vector<disruptor::Sequence*>& sequences) {
    // Guard against Windows macro max()
    return getMinimumSequence(sequences, (std::numeric_limits<int64_t>::max)());
  }

  static int64_t getMinimumSequence(const std::vector<disruptor::Sequence*>& sequences,
                                    int64_t minimum) {
    int64_t minimumSequence = minimum;
    for (auto* seq : sequences) {
      if (!seq) {
        continue;
      }
      int64_t value = seq->get();
      // Guard against Windows macro min()
      minimumSequence = (std::min)(minimumSequence, value);
    }
    return minimumSequence;
  }

  static std::vector<disruptor::Sequence*> getSequencesFor(disruptor::EventProcessor* const* processors, int count) {
    std::vector<disruptor::Sequence*> sequences;
    sequences.reserve(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
      auto* p = processors[i];
      sequences.push_back(p ? &p->getSequence() : nullptr);
    }
    return sequences;
  }

  static int64_t currentTimeMillis() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
  }

  static std::vector<disruptor::Sequence*> getSequencesFor(
      const std::vector<disruptor::EventProcessor*>& processors) {
    std::vector<disruptor::Sequence*> sequences;
    sequences.reserve(processors.size());
    for (auto* p : processors) {
      sequences.push_back(&p->getSequence());
    }
    return sequences;
  }

  static int log2(int value) {
    if (value < 1) {
      throw std::invalid_argument("value must be a positive number");
    }
    // Java: Integer.SIZE - Integer.numberOfLeadingZeros(value) - 1
    uint32_t v = static_cast<uint32_t>(value);
    int leading = 0;
#if defined(_MSC_VER)
    unsigned long idx = 0;
    _BitScanReverse(&idx, v);
    leading = 31 - static_cast<int>(idx);
#else
    leading = __builtin_clz(v);
#endif
    return 32 - leading - 1;
  }

  // Java:
  // public static long awaitNanos(final Object mutex, final long timeoutNanos) throws InterruptedException
  static int64_t awaitNanos(std::condition_variable& cv,
                           std::unique_lock<std::mutex>& lock,
                           int64_t timeoutNanos) {
    const auto millis = timeoutNanos / ONE_MILLISECOND_IN_NANOSECONDS;
    const auto nanos = timeoutNanos % ONE_MILLISECOND_IN_NANOSECONDS;

    auto t0 = std::chrono::steady_clock::now();
    cv.wait_for(lock, std::chrono::milliseconds(millis) + std::chrono::nanoseconds(nanos));
    auto t1 = std::chrono::steady_clock::now();

    auto waited = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    return timeoutNanos - static_cast<int64_t>(waited);
  }
};

} // namespace disruptor::util
