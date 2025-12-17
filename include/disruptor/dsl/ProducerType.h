#pragma once
// 1:1 port of com.lmax.disruptor.dsl.ProducerType
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/dsl/ProducerType.java

namespace disruptor::dsl {

// Defines producer types to support creation of RingBuffer with correct sequencer and publisher.
enum class ProducerType {
  // Create a RingBuffer with a single event publisher to the RingBuffer
  SINGLE,
  // Create a RingBuffer supporting multiple event publishers to the one RingBuffer
  MULTI
};

} // namespace disruptor::dsl


