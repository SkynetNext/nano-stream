// 1:1-ish port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/longevent/lambdas/LongEventMain.java
//
// Java loops forever; C++ port runs a bounded number of iterations.

#include "disruptor/EventTranslatorTwoArg.h"
#include "disruptor/BlockingWaitStrategy.h"
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

class PrintHandler final : public disruptor::EventHandler<disruptor_examples::longevent::LongEvent> {
public:
  void onEvent(disruptor_examples::longevent::LongEvent& event, int64_t /*sequence*/, bool /*endOfBatch*/) override {
    std::cout << "Event: " << event.toString() << "\n";
  }
};

class Translator final
    : public disruptor::EventTranslatorOneArg<disruptor_examples::longevent::LongEvent, disruptor_examples::longevent::ByteBuffer> {
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
  using Event = disruptor_examples::longevent::LongEvent;
  using WS = disruptor::BlockingWaitStrategy;
  constexpr auto Producer = disruptor::dsl::ProducerType::MULTI;
  using DisruptorT = disruptor::dsl::Disruptor<Event, Producer, WS>;

  WS ws;
  DisruptorT disruptor(factory, bufferSize, tf, ws);

  PrintHandler handler;
  disruptor.handleEventsWith(handler);
  disruptor.start();

  auto& ringBuffer = disruptor.getRingBuffer();
  disruptor_examples::longevent::ByteBuffer bb = disruptor_examples::longevent::ByteBuffer::allocate(8);
  Translator tr;

  for (int64_t l = 0; l < 5; ++l) {
    bb.putLong(0, l);
    ringBuffer.publishEvent(tr, bb);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  disruptor.shutdown(2000);
  return 0;
}


