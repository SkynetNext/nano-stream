// 1:1-ish port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/EarlyReleaseHandler.java
//
// Java source is a handler example; this C++ port provides a small runnable main() demonstrating it.

#include "disruptor/BatchEventProcessorBuilder.h"
#include "disruptor/BlockingWaitStrategy.h"
#include "disruptor/RingBuffer.h"
#include "disruptor/util/DaemonThreadFactory.h"

#include "support/LongEvent.h"

#include <cstdint>
#include <memory>
#include <thread>

namespace {

class EarlyReleaseHandler final : public disruptor::EventHandler<disruptor_examples::support::LongEvent> {
public:
  void setSequenceCallback(disruptor::Sequence& sequenceCallback) override { sequenceCallback_ = &sequenceCallback; }

  void onEvent(disruptor_examples::support::LongEvent& event, int64_t sequence, bool endOfBatch) override {
    processEvent(event);

    bool logicalChunkOfWorkComplete = isLogicalChunkOfWorkComplete();
    if (logicalChunkOfWorkComplete && sequenceCallback_ != nullptr) {
      sequenceCallback_->set(sequence);
    }

    batchRemaining_ = (logicalChunkOfWorkComplete || endOfBatch) ? 20 : batchRemaining_;
  }

private:
  disruptor::Sequence* sequenceCallback_{nullptr};
  int batchRemaining_{20};

  bool isLogicalChunkOfWorkComplete() { return --batchRemaining_ == -1; }
  static void processEvent(disruptor_examples::support::LongEvent& /*event*/) {
    // Do processing
  }
};

} // namespace

int main() {
  using Event = disruptor_examples::support::LongEvent;
  using WS = disruptor::BlockingWaitStrategy;
  using Seq = disruptor::SingleProducerSequencer<WS>;
  using RingBufferT = disruptor::RingBuffer<Event, Seq>;

  WS ws;
  auto rb = RingBufferT::createSingleProducer(Event::FACTORY, 1024, ws);

  auto barrier = rb->newBarrier();
  EarlyReleaseHandler handler;
  disruptor::BatchEventProcessorBuilder builder;
  auto processor = builder.build(*rb, *barrier, handler);

  rb->addGatingSequences(processor->getSequence());

  std::thread t([&] { processor->run(); });

  for (int i = 0; i < 100; ++i) {
    int64_t seq = rb->next();
    rb->get(seq).set(seq);
    rb->publish(seq);
  }

  processor->halt();
  if (t.joinable()) t.join();
  return 0;
}


