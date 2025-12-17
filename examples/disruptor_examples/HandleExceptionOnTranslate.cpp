// 1:1-ish port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/HandleExceptionOnTranslate.java

#include "disruptor/dsl/Disruptor.h"
#include "disruptor/BlockingWaitStrategy.h"
#include "disruptor/util/DaemonThreadFactory.h"

#include "support/LongEvent.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <thread>

namespace {
constexpr int64_t NO_VALUE_SPECIFIED = -1;

class MyHandler final : public disruptor::EventHandler<disruptor_examples::support::LongEvent> {
public:
  void onEvent(disruptor_examples::support::LongEvent& event, int64_t sequence, bool /*endOfBatch*/) override {
    if (event.get() == NO_VALUE_SPECIFIED) {
      std::printf("Discarded\n");
    } else {
      std::printf("Processed: %s\n", (event.get() == sequence) ? "true" : "false");
    }
  }
};

} // namespace

int main() {
  auto& tf = disruptor::util::DaemonThreadFactory::INSTANCE();

  using Event = disruptor_examples::support::LongEvent;
  using WS = disruptor::BlockingWaitStrategy;
  constexpr auto Producer = disruptor::dsl::ProducerType::MULTI;
  using DisruptorT = disruptor::dsl::Disruptor<Event, Producer, WS>;

  WS ws;
  DisruptorT disruptor(Event::FACTORY, 1024, tf, ws);

  MyHandler handler;
  disruptor.handleEventsWith(handler);
  auto rb = disruptor.start();

  for (int i = 0; i < 10; ++i) {
    int64_t sequence = rb->next();
    try {
      auto& event = rb->get(sequence);
      event.set(NO_VALUE_SPECIFIED);
      if (sequence % 3 == 0) {
        throw std::runtime_error("Skipping");
      }
      event.set(sequence);
    } catch (...) {
      // Skipping
    }
    // Java publishEvent always publishes in finally; do the same here.
    rb->publish(sequence);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(5000));
  disruptor.shutdown();
  return 0;
}


