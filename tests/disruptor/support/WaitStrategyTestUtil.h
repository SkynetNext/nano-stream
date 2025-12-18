#pragma once
// 1:1 port of com.lmax.disruptor.support.WaitStrategyTestUtil
// Updated for template-based architecture: template on WaitStrategy type

#include "disruptor/Sequence.h"

#include "DummySequenceBarrier.h"
#include "SequenceUpdater.h"

#include <cstdint>
#include <thread>

namespace disruptor::support {

struct WaitStrategyTestUtil {
  template <typename WaitStrategyT>
  static void assertWaitForWithDelayOf(int64_t sleepTimeMillis, WaitStrategyT& waitStrategy) {
    SequenceUpdater<WaitStrategyT> sequenceUpdater(sleepTimeMillis, waitStrategy);
    std::thread t([&] { sequenceUpdater.run(); });

    sequenceUpdater.waitForStartup();

    ::disruptor::Sequence cursor(0);
    DummySequenceBarrier barrier;
    int64_t sequence = waitStrategy.waitFor(0, cursor, sequenceUpdater.sequence, barrier);

    if (t.joinable()) {
      t.join();
    }

    // Java: assertThat(sequence, is(0L));
    if (sequence != 0) {
      throw std::runtime_error("WaitStrategyTestUtil: expected sequence == 0");
    }
  }
};

} // namespace disruptor::support


