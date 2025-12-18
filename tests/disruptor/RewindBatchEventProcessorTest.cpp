#include <gtest/gtest.h>

#include "disruptor/BatchEventProcessorBuilder.h"
#include "disruptor/BusySpinWaitStrategy.h"
#include "disruptor/RingBuffer.h"
#include "disruptor/SimpleBatchRewindStrategy.h"
#include "tests/disruptor/support/LongEvent.h"

#include <thread>

namespace {
class NoThrowRewindableHandler final : public disruptor::RewindableEventHandler<disruptor::support::LongEvent> {
public:
  void onEvent(disruptor::support::LongEvent& /*event*/, int64_t /*sequence*/, bool /*endOfBatch*/) override {}
};
} // namespace

TEST(RewindBatchEventProcessorTest, shouldRunWithRewindableHandler_smoke) {
  using Event = disruptor::support::LongEvent;
  using WS = disruptor::BusySpinWaitStrategy;
  using RB = disruptor::MultiProducerRingBuffer<Event, WS>;
  WS ws;
  auto ringBuffer = RB::createMultiProducer(disruptor::support::LongEvent::FACTORY, 1024, ws);
  auto barrier = ringBuffer->newBarrier(nullptr, 0);

  NoThrowRewindableHandler handler;
  disruptor::SimpleBatchRewindStrategy strategy;
  disruptor::BatchEventProcessorBuilder builder;
  auto processor = builder.build(*ringBuffer, *barrier, handler, strategy);
  ringBuffer->addGatingSequences(processor->getSequence());

  // Publish a few events, then run processor once.
  for (int i = 0; i < 8; ++i) {
    int64_t s = ringBuffer->next();
    ringBuffer->get(s).set(s);
    ringBuffer->publish(s);
  }

  std::thread t([&] { processor->run(); });
  std::this_thread::sleep_for(std::chrono::milliseconds(10));
  processor->halt();
  t.join();

  SUCCEED();
}
