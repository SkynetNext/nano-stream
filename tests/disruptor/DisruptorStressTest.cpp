#include <gtest/gtest.h>

#include "disruptor/BusySpinWaitStrategy.h"
#include "disruptor/FatalExceptionHandler.h"
#include "disruptor/dsl/Disruptor.h"
#include "disruptor/dsl/ProducerType.h"
#include "disruptor/util/DaemonThreadFactory.h"

#include "tests/disruptor/test_support/CountDownLatch.h"

#include <barrier>
#include <string>
#include <thread>
#include <vector>

namespace {
struct TestEvent {
  int64_t sequence{0};
  int64_t a{0};
  int64_t b{0};
  std::string s{};

  struct Factory final : public disruptor::EventFactory<TestEvent> {
    TestEvent newInstance() override { return TestEvent(); }
  };
  static inline std::shared_ptr<disruptor::EventFactory<TestEvent>> FACTORY = std::make_shared<Factory>();
};

class TestEventHandler final : public disruptor::EventHandler<TestEvent> {
public:
  void onEvent(TestEvent& event, int64_t sequence, bool /*endOfBatch*/) override {
    if (event.sequence != sequence || event.a != sequence + 13 || event.b != sequence - 7 || event.s != ("wibble-" + std::to_string(sequence))) {
      ++failureCount;
    }
    ++messagesSeen;
  }

  int failureCount{0};
  int messagesSeen{0};
};
} // namespace

TEST(DisruptorStressTest, shouldHandleLotsOfThreads_smoke) {
  auto& tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  auto ws = std::make_shared<disruptor::BusySpinWaitStrategy>();
  disruptor::dsl::Disruptor<TestEvent> d(TestEvent::FACTORY, 1 << 12, tf, disruptor::dsl::ProducerType::MULTI, ws);

  disruptor::FatalExceptionHandler<TestEvent> fatal;
  d.setDefaultExceptionHandler(fatal);

  constexpr int threads = 2;
  constexpr int iterations = 2000;
  std::barrier startBarrier(threads);
  disruptor::test_support::CountDownLatch done(threads);

  std::vector<TestEventHandler> handlers;
  handlers.reserve(threads);
  for (int i = 0; i < threads; ++i) {
    handlers.emplace_back();
    d.handleEventsWith(handlers.back());
  }

  auto rb = d.start();

  struct Publisher {
    std::shared_ptr<disruptor::RingBuffer<TestEvent>> rb;
    int iterations;
    std::barrier<>* barrier;
    disruptor::test_support::CountDownLatch* done;
    bool failed{false};
    void operator()() {
      try {
        barrier->arrive_and_wait();
        for (int i = 0; i < iterations; ++i) {
          int64_t next = rb->next();
          auto& e = rb->get(next);
          e.sequence = next;
          e.a = next + 13;
          e.b = next - 7;
          e.s = "wibble-" + std::to_string(next);
          rb->publish(next);
        }
      } catch (...) {
        failed = true;
      }
      done->countDown();
    }
  };

  std::vector<std::thread> pubThreads;
  std::vector<std::unique_ptr<Publisher>> pubs;
  for (int i = 0; i < threads; ++i) {
    pubs.emplace_back(std::make_unique<Publisher>(Publisher{rb, iterations, &startBarrier, &done}));
    pubThreads.emplace_back([p = pubs.back().get()] { (*p)(); });
  }

  done.await();
  d.shutdown(2000);
  for (auto& t : pubThreads) t.join();

  for (auto& p : pubs) {
    EXPECT_FALSE(p->failed);
  }
  for (auto& h : handlers) {
    EXPECT_GT(h.messagesSeen, 0);
    EXPECT_EQ(h.failureCount, 0);
  }
}
