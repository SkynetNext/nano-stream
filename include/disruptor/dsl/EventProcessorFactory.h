#pragma once
// 1:1 port of com.lmax.disruptor.dsl.EventProcessorFactory
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/dsl/EventProcessorFactory.java

#include "../EventProcessor.h"
#include "../RingBuffer.h"
#include "../Sequence.h"

namespace disruptor::dsl {

template <typename T>
class EventProcessorFactory {
public:
  virtual ~EventProcessorFactory() = default;

  virtual EventProcessor& createEventProcessor(RingBuffer<T>& ringBuffer, Sequence* const* barrierSequences, int count) = 0;
};

} // namespace disruptor::dsl


