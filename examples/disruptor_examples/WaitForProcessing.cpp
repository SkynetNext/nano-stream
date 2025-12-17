// 1:1 port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/WaitForProcessing.java

#include "disruptor/dsl/Disruptor.h"
#include "disruptor/BlockingWaitStrategy.h"
#include "disruptor/util/DaemonThreadFactory.h"

#include "support/LongEvent.h"

#include <cstdint>
#include <thread>

namespace {
class Consumer final : public disruptor::EventHandler<disruptor_examples::support::LongEvent> {
public:
  void onEvent(disruptor_examples::support::LongEvent& /*event*/, int64_t /*sequence*/, bool /*endOfBatch*/) override {}
};

class Translator final : public disruptor::EventTranslator<disruptor_examples::support::LongEvent> {
public:
  void translateTo(disruptor_examples::support::LongEvent& event, int64_t sequence) override { event.set(sequence - 4); }
};

template <typename RingBufferT>
static void waitForRingBufferToBeIdle(RingBufferT& ringBuffer) {
  while (ringBuffer.getBufferSize() - ringBuffer.remainingCapacity() != 0) {
    // Wait for processing...
    std::this_thread::yield();
  }
}

template <typename DisruptorT, typename RingBufferT>
static void waitForSpecificConsumer(DisruptorT& disruptor, Consumer& lastConsumer, RingBufferT& ringBuffer) {
  int64_t lastPublishedValue;
  int64_t sequenceValueFor;
  do {
    lastPublishedValue = ringBuffer.getCursor();
    sequenceValueFor = disruptor.getSequenceValueFor(lastConsumer);
  } while (sequenceValueFor < lastPublishedValue);
}
} // namespace

int main() {
  auto& tf = disruptor::util::DaemonThreadFactory::INSTANCE();

  using Event = disruptor_examples::support::LongEvent;
  using WS = disruptor::BlockingWaitStrategy;
  constexpr auto Producer = disruptor::dsl::ProducerType::MULTI;
  using DisruptorT = disruptor::dsl::Disruptor<Event, Producer, WS>;

  WS ws;
  DisruptorT disruptor(Event::FACTORY, 1024, tf, ws);

  Consumer firstConsumer;
  Consumer lastConsumer;
  disruptor.handleEventsWith(firstConsumer).then(lastConsumer);

  auto& ringBuffer = disruptor.getRingBuffer();

  Translator translator;
  ringBuffer.tryPublishEvent(translator);

  waitForSpecificConsumer(disruptor, lastConsumer, ringBuffer);
  waitForRingBufferToBeIdle(ringBuffer);

  disruptor.shutdown();
  return 0;
}


