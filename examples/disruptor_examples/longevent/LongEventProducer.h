#pragma once
// 1:1 port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/longevent/LongEventProducer.java

#include "disruptor/EventTranslatorOneArg.h"
#include "disruptor/RingBuffer.h"

#include "ByteBuffer.h"
#include "LongEvent.h"

#include <cstdint>

namespace disruptor_examples::longevent {

class LongEventProducer {
public:
  explicit LongEventProducer(disruptor::RingBuffer<LongEvent>& ringBuffer) : ringBuffer_(&ringBuffer) {}

  void onData(ByteBuffer& bb) { ringBuffer_->publishEvent(TRANSLATOR, bb); }

private:
  disruptor::RingBuffer<LongEvent>* ringBuffer_;

  class Translator final : public disruptor::EventTranslatorOneArg<LongEvent, ByteBuffer> {
  public:
    void translateTo(LongEvent& event, int64_t /*sequence*/, ByteBuffer bb) override { event.set(bb.getLong(0)); }
  };

  static inline Translator TRANSLATOR{};
};

} // namespace disruptor_examples::longevent


