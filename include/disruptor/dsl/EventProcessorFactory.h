#pragma once
// 1:1 port of com.lmax.disruptor.dsl.EventProcessorFactory
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/dsl/EventProcessorFactory.java

#include "../EventProcessor.h"
#include "../RingBuffer.h"
#include "../Sequence.h"

#include <memory>

namespace disruptor::dsl {

template <typename T, typename RingBufferT>
class EventProcessorFactory {
public:
  virtual ~EventProcessorFactory() = default;

  virtual EventProcessor& createEventProcessor(RingBufferT& ringBuffer, Sequence* const* barrierSequences, int count) = 0;

  // Optional method to return shared_ptr for lifetime management.
  // Default implementation returns nullptr; override if factory manages processor with shared_ptr.
  virtual std::shared_ptr<EventProcessor> getProcessorSharedPtr(EventProcessor& processor) {
    (void)processor;
    return nullptr;
  }
};

} // namespace disruptor::dsl


