// 1:1 port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/WaitForShutdown.java

#include "disruptor/TimeoutException.h"
#include "disruptor/dsl/Disruptor.h"
#include "disruptor/util/DaemonThreadFactory.h"

#include "support/LongEvent.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <thread>

namespace {
std::atomic<int> g_value{0};

class Handler final : public disruptor::EventHandler<disruptor_examples::support::LongEvent> {
public:
  explicit Handler(std::atomic<int>& shutdownCountdown) : shutdownCountdown_(&shutdownCountdown) {}

  void onShutdown() override { shutdownCountdown_->fetch_sub(1, std::memory_order_acq_rel); }

  void onEvent(disruptor_examples::support::LongEvent& /*event*/, int64_t /*sequence*/, bool /*endOfBatch*/) override {
    g_value.store(1, std::memory_order_release);
  }

private:
  std::atomic<int>* shutdownCountdown_;
};

static void spinWaitCountdown(std::atomic<int>& countdown) {
  while (countdown.load(std::memory_order_acquire) > 0) {
    std::this_thread::yield();
  }
}
} // namespace

int main() {
  try {
    auto& tf = disruptor::util::DaemonThreadFactory::INSTANCE();

    disruptor::dsl::Disruptor<disruptor_examples::support::LongEvent,
                              disruptor::dsl::ProducerType::MULTI,
                              disruptor::BlockingWaitStrategy>
        disruptor(disruptor_examples::support::LongEvent::FACTORY, 16, tf,
                  disruptor::BlockingWaitStrategy{});

    // Java uses CountDownLatch(2) for two handlers in chain.
    std::atomic<int> shutdownCountdown{2};

    Handler h1(shutdownCountdown);
    Handler h2(shutdownCountdown);
    disruptor.handleEventsWith(h1).then(h2);
    disruptor.start();

    auto& rb = disruptor.getRingBuffer();
    int64_t next = rb.next();
    rb.get(next).set(next);
    rb.publish(next);

    // Java: disruptor.shutdown(10, TimeUnit.SECONDS);
    disruptor.shutdown(10'000);

    spinWaitCountdown(shutdownCountdown);

    std::cout << g_value.load(std::memory_order_acquire) << "\n";
    return 0;
  } catch (const disruptor::TimeoutException&) {
    std::cerr << "TimeoutException\n";
    return 2;
  }
}


