#pragma once
// 1:1 port of com.lmax.disruptor.dsl.ConsumerInfo (package-private interface)
// Source:
// reference/disruptor/src/main/java/com/lmax/disruptor/dsl/ConsumerInfo.java

#include "../Sequence.h"
#include "ThreadFactory.h"

#include <latch>

namespace disruptor::dsl {

template <typename BarrierPtrT> class ConsumerInfo {
public:
  virtual ~ConsumerInfo() = default;

  virtual Sequence *const *getSequences() = 0;
  virtual int getSequenceCount() const = 0;

  virtual BarrierPtrT getBarrier() = 0;
  virtual bool isEndOfChain() = 0;

  virtual void start(ThreadFactory &threadFactory,
                     std::latch *startupLatch = nullptr) = 0;
  virtual void halt() = 0;
  virtual void join() = 0;
  virtual void markAsUsedInBarrier() = 0;
  virtual bool isRunning() = 0;
};

} // namespace disruptor::dsl
