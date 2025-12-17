// 1:1-ish port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/ShutdownOnError.java
//
// Java version demonstrates wiring a per-handler ExceptionHandler that can decide fatality.
// Java's sample loops forever; this C++ port runs a bounded number of publishes.

#include "disruptor/ExceptionHandler.h"
#include "disruptor/BlockingWaitStrategy.h"
#include "disruptor/dsl/Disruptor.h"
#include "disruptor/util/DaemonThreadFactory.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <stdexcept>

namespace {

struct Event {
  int64_t value{0};
};

struct Factory final : public disruptor::EventFactory<Event> {
  Event newInstance() override { return Event(); }
};

class Handler final : public disruptor::EventHandler<Event> {
public:
  void onEvent(Event& /*event*/, int64_t /*sequence*/, bool /*endOfBatch*/) override {
    // do work, if a failure occurs throw exception.
  }
};

class ErrorHandler final : public disruptor::ExceptionHandler<Event> {
public:
  explicit ErrorHandler(std::atomic<bool>& running) : running_(&running) {}

  void handleEventException(const std::exception& ex, int64_t /*sequence*/, Event* /*event*/) override {
    if (exceptionIsFatal(ex)) {
      // Fatal -> stop publishing and rethrow (matches example intent).
      running_->store(false, std::memory_order_release);
      throw std::runtime_error(ex.what());
    }
  }

  void handleOnStartException(const std::exception& /*ex*/) override {}
  void handleOnShutdownException(const std::exception& /*ex*/) override {}

private:
  std::atomic<bool>* running_;
  static bool exceptionIsFatal(const std::exception& /*ex*/) { return true; }
};

} // namespace

int main() {
  auto& tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  auto factory = std::make_shared<Factory>();
  using WS = disruptor::BlockingWaitStrategy;
  constexpr auto Producer = disruptor::dsl::ProducerType::MULTI;
  using DisruptorT = disruptor::dsl::Disruptor<Event, Producer, WS>;

  WS ws;
  DisruptorT disruptor(factory, 1024, tf, ws);

  std::atomic<bool> running{true};
  ErrorHandler errorHandler(running);

  Handler handler;
  disruptor.handleEventsWith(handler);
  disruptor.handleExceptionsFor(handler).with(errorHandler);
  auto rb = disruptor.start();

  // simplePublish
  for (int i = 0; i < 10'000 && running.load(std::memory_order_acquire); ++i) {
    int64_t seq = rb->next();
    rb->get(seq).value = seq;
    rb->publish(seq);
  }

  disruptor.shutdown(2000);
  return 0;
}


