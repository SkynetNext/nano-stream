// 1:1 port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/DynamicallyAddHandler.java

#include "disruptor/BatchEventProcessorBuilder.h"
#include "disruptor/dsl/Disruptor.h"
#include "disruptor/util/DaemonThreadFactory.h"

#include "support/StubEvent.h"

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace {

class DynamicHandler final
    : public disruptor::EventHandler<disruptor_examples::support::StubEvent> {
public:
  void onEvent(disruptor_examples::support::StubEvent & /*event*/,
               int64_t /*sequence*/, bool /*endOfBatch*/) override {}

  void onShutdown() override {
    std::lock_guard<std::mutex> lk(mu_);
    shutdown_ = true;
    cv_.notify_all();
  }

  void awaitShutdown() {
    std::unique_lock<std::mutex> lk(mu_);
    cv_.wait(lk, [&] { return shutdown_; });
  }

private:
  std::mutex mu_;
  std::condition_variable cv_;
  bool shutdown_{false};
};

} // namespace

int main() {
  auto &tf = disruptor::util::DaemonThreadFactory::INSTANCE();

  // Build a disruptor and start it.
  disruptor::dsl::Disruptor<disruptor_examples::support::StubEvent,
                            disruptor::dsl::ProducerType::MULTI,
                            disruptor::BlockingWaitStrategy>
      disruptor(disruptor_examples::support::StubEvent::EVENT_FACTORY, 1024,
                tf);
  auto ringBuffer = disruptor.start();

  // Construct 2 batch event processors.
  disruptor::BatchEventProcessorBuilder builder;

  auto barrier1 = ringBuffer->newBarrier();
  DynamicHandler handler1;
  auto processor1 = builder.build(*ringBuffer, *barrier1, handler1);

  disruptor::Sequence *deps[1] = {&processor1->getSequence()};
  auto barrier2 = ringBuffer->newBarrier(deps, 1);
  DynamicHandler handler2;
  auto processor2 = builder.build(*ringBuffer, *barrier2, handler2);

  // Dynamically add both sequences to the ring buffer
  ringBuffer->addGatingSequences(processor1->getSequence());
  ringBuffer->addGatingSequences(processor2->getSequence());

  // Start the new batch processors.
  std::thread t1([&] { processor1->run(); });
  std::thread t2([&] { processor2->run(); });

  // Remove a processor.
  processor2->halt();
  handler2.awaitShutdown();
  ringBuffer->removeGatingSequence(processor2->getSequence());

  // Clean shutdown.
  processor1->halt();
  handler1.awaitShutdown();
  if (t1.joinable())
    t1.join();
  if (t2.joinable())
    t2.join();
  disruptor.halt();
  return 0;
}
