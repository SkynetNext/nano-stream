#include <gtest/gtest.h>

#include "disruptor/BlockingWaitStrategy.h"
#include "disruptor/FatalExceptionHandler.h"
#include "disruptor/dsl/Disruptor.h"
#include "disruptor/dsl/ProducerType.h"
#include "disruptor/util/DaemonThreadFactory.h"

#include <random>
#include <vector>

namespace {
class ByteArrayTranslator final : public disruptor::EventTranslator<std::vector<uint8_t>> {
public:
  explicit ByteArrayTranslator(std::vector<uint8_t> bytes) : bytes_(std::move(bytes)) {}
  void translateTo(std::vector<uint8_t>& event, int64_t /*sequence*/) override { event = bytes_; }
private:
  std::vector<uint8_t> bytes_;
};

class FailingEventHandler final : public disruptor::EventHandler<std::vector<uint8_t>> {
public:
  void onEvent(std::vector<uint8_t>& /*event*/, int64_t /*sequence*/, bool /*endOfBatch*/) override {
    if (++count_ == 3) {
      throw std::runtime_error("fail");
    }
  }
private:
  int count_{0};
};

class ByteArrayFactory final : public disruptor::EventFactory<std::vector<uint8_t>> {
public:
  explicit ByteArrayFactory(size_t n) : n_(n) {}
  std::vector<uint8_t> newInstance() override { return std::vector<uint8_t>(n_); }
private:
  size_t n_;
};
} // namespace

TEST(ShutdownOnFatalExceptionTest, shouldShutdownGracefulEvenWithFatalExceptionHandler) {
  std::mt19937 rng(123);
  std::uniform_int_distribution<int> dist(0, 255);

  auto& tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  auto ws = std::make_shared<disruptor::BlockingWaitStrategy>();
  FailingEventHandler handler;

  disruptor::dsl::Disruptor<std::vector<uint8_t>> d(
      std::make_shared<ByteArrayFactory>(256), 1024, tf, disruptor::dsl::ProducerType::SINGLE, ws);
  d.handleEventsWith(handler);

  disruptor::FatalExceptionHandler<std::vector<uint8_t>> fatal;
  d.setDefaultExceptionHandler(fatal);

  d.start();
  for (int i = 1; i < 10; ++i) {
    std::vector<uint8_t> bytes(32);
    for (auto& b : bytes) b = static_cast<uint8_t>(dist(rng));
    ByteArrayTranslator tr(bytes);
    d.publishEvent(tr);
  }
  // Java test relies on a test-level timeout and calls disruptor.shutdown() in tearDown.
  // C++ equivalent: call shutdown() (which swallows TimeoutException) and ensure it returns.
  d.shutdown();
}
