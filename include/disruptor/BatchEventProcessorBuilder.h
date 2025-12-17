#pragma once
// 1:1 port of com.lmax.disruptor.BatchEventProcessorBuilder
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/BatchEventProcessorBuilder.java

#include "BatchEventProcessor.h"
#include "BatchRewindStrategy.h"
#include "DataProvider.h"
#include "EventHandler.h"
#include "RewindableEventHandler.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>

namespace disruptor {

class BatchEventProcessorBuilder final {
public:
  BatchEventProcessorBuilder() = default;

  BatchEventProcessorBuilder& setMaxBatchSize(int maxBatchSize) {
    maxBatchSize_ = maxBatchSize;
    return *this;
  }

  template <typename T, typename BarrierT>
  std::shared_ptr<BatchEventProcessor<T, BarrierT>> build(DataProvider<T>& dataProvider,
                                                          BarrierT& sequenceBarrier,
                                                          EventHandler<T>& eventHandler) {
    auto processor = std::make_shared<BatchEventProcessor<T, BarrierT>>(dataProvider,
                                                              sequenceBarrier,
                                                              eventHandler,
                                                              maxBatchSize_,
                                                              nullptr);
    eventHandler.setSequenceCallback(processor->getSequence());
    return processor;
  }

  template <typename T, typename BarrierT>
  std::shared_ptr<BatchEventProcessor<T, BarrierT>> build(DataProvider<T>& dataProvider,
                                                          BarrierT& sequenceBarrier,
                                                          RewindableEventHandler<T>& rewindableEventHandler,
                                                          BatchRewindStrategy& batchRewindStrategy) {
    // Java: NPE if batchRewindStrategy null (we take ref so never null).
    return std::make_shared<BatchEventProcessor<T, BarrierT>>(dataProvider,
                                                    sequenceBarrier,
                                                    rewindableEventHandler,
                                                    maxBatchSize_,
                                                    &batchRewindStrategy);
  }

private:
  int maxBatchSize_ = std::numeric_limits<int>::max();
};

} // namespace disruptor
