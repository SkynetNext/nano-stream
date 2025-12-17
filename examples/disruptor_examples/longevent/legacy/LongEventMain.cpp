// 1:1-ish port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/longevent/legacy/LongEventMain.java
//
// Java loops forever; C++ port runs a bounded number of iterations.

#include "disruptor/RingBuffer.h"
#include "disruptor/dsl/Disruptor.h"
#include "disruptor/util/DaemonThreadFactory.h"

#include "../LongEvent.h"
#include "../LongEventFactory.h"
#include "../LongEventHandler.h"
#include "../ByteBuffer.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>

namespace {
using WS = disruptor::BlockingWaitStrategy;
using Seq = disruptor::MultiProducerSequencer<WS>;
using RB = disruptor::RingBuffer<disruptor_examples::longevent::LongEvent, Seq>;

class LegacyProducer {
public:
  explicit LegacyProducer(RB& ringBuffer) : rb_(&ringBuffer) {}

  void onData(disruptor_examples::longevent::ByteBuffer& bb) {
    int64_t sequence = rb_->next();
    try {
      auto& event = rb_->get(sequence);
      event.set(bb.getLong(0));
    } catch (...) {
      rb_->publish(sequence);
      throw;
    }
    rb_->publish(sequence);
  }

private:
  RB* rb_;
};
} // namespace

int main() {
  using WS = disruptor::BlockingWaitStrategy;
  using D = disruptor::dsl::Disruptor<disruptor_examples::longevent::LongEvent, disruptor::dsl::ProducerType::MULTI, WS>;
  disruptor_examples::longevent::LongEventFactory factory;
  constexpr int bufferSize = 1024;
  auto& tf = disruptor::util::DaemonThreadFactory::INSTANCE();

  auto factoryPtr = std::make_shared<disruptor_examples::longevent::LongEventFactory>(factory);
  WS ws;
  D disruptor(factoryPtr, bufferSize, tf, ws);

  disruptor_examples::longevent::LongEventHandler handler;
  disruptor.handleEventsWith(handler);
  disruptor.start();

  auto& ringBuffer = disruptor.getRingBuffer();
  LegacyProducer producer(ringBuffer);
  auto bb = disruptor_examples::longevent::ByteBuffer::allocate(8);

  for (int64_t l = 0; l < 5; ++l) {
    bb.putLong(0, l);
    producer.onData(bb);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  disruptor.shutdown(2000);
  return 0;
}


