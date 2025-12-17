// 1:1-ish port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/longevent/methodrefs/LongEventMain.java
//
// Java loops forever; C++ port runs a bounded number of iterations.

#include "disruptor/EventTranslatorOneArg.h"
#include "disruptor/RingBuffer.h"
#include "disruptor/dsl/Disruptor.h"
#include "disruptor/util/DaemonThreadFactory.h"

#include "../LongEvent.h"
#include "../ByteBuffer.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>

namespace {
struct Factory final : public disruptor::EventFactory<disruptor_examples::longevent::LongEvent> {
  disruptor_examples::longevent::LongEvent newInstance() override { return disruptor_examples::longevent::LongEvent(); }
};

static void handleEvent(disruptor_examples::longevent::LongEvent& event, int64_t /*sequence*/, bool /*endOfBatch*/) {
  std::cout << event.toString() << "\n";
}

class Translate final : public disruptor::EventTranslatorOneArg<disruptor_examples::longevent::LongEvent, disruptor_examples::longevent::ByteBuffer> {
public:
  void translateTo(disruptor_examples::longevent::LongEvent& event, int64_t /*sequence*/, disruptor_examples::longevent::ByteBuffer buffer) override {
    event.set(buffer.getLong(0));
  }
};
} // namespace

int main() {
  constexpr int bufferSize = 1024;
  auto& tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  auto factory = std::make_shared<Factory>();

  disruptor::dsl::Disruptor<disruptor_examples::longevent::LongEvent> disruptor(factory, bufferSize, tf);
  // No direct method reference equivalent; wrap with a handler.
  class Handler final : public disruptor::EventHandler<disruptor_examples::longevent::LongEvent> {
  public:
    void onEvent(disruptor_examples::longevent::LongEvent& event, int64_t sequence, bool endOfBatch) override {
      handleEvent(event, sequence, endOfBatch);
    }
  } handler;
  disruptor.handleEventsWith(handler);
  disruptor.start();

  auto& ringBuffer = disruptor.getRingBuffer();
  disruptor_examples::longevent::ByteBuffer bb = disruptor_examples::longevent::ByteBuffer::allocate(8);
  Translate tr;

  for (int64_t l = 0; l < 5; ++l) {
    bb.putLong(0, l);
    ringBuffer.publishEvent(tr, bb);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  disruptor.shutdown(2000);
  return 0;
}


