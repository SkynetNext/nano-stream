// 1:1-ish port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/MultiProducerWithTranslator.java
//
// NOTE: Java version loops forever. This C++ port runs a bounded number of iterations.

#include "disruptor/BlockingWaitStrategy.h"
#include "disruptor/EventTranslatorThreeArg.h"
#include "disruptor/dsl/Disruptor.h"
#include "disruptor/dsl/ProducerType.h"
#include "disruptor/util/DaemonThreadFactory.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace {

struct IMessage {};
struct ITransportable {};

struct ObjectBox {
  IMessage* message{nullptr};
  ITransportable* transportable{nullptr};
  std::string string{};

  struct Factory final : public disruptor::EventFactory<ObjectBox> {
    ObjectBox newInstance() override { return ObjectBox(); }
  };

  static inline std::shared_ptr<disruptor::EventFactory<ObjectBox>> FACTORY = std::make_shared<Factory>();

  void setMessage(IMessage& arg0) { message = &arg0; }
  void setTransportable(ITransportable& arg1) { transportable = &arg1; }
  void setStreamName(const std::string& arg2) { string = arg2; }
};

class Publisher final : public disruptor::EventTranslatorThreeArg<ObjectBox, IMessage*, ITransportable*, std::string> {
public:
  void translateTo(ObjectBox& event, int64_t /*sequence*/, IMessage* arg0, ITransportable* arg1, std::string arg2) override {
    if (arg0) event.setMessage(*arg0);
    if (arg1) event.setTransportable(*arg1);
    event.setStreamName(arg2);
  }
};

class Consumer final : public disruptor::EventHandler<ObjectBox> {
public:
  void onEvent(ObjectBox& /*event*/, int64_t /*sequence*/, bool /*endOfBatch*/) override {}
};

constexpr int kRingSize = 1024;

} // namespace

int main() {
  auto& tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  auto ws = std::make_shared<disruptor::BlockingWaitStrategy>();

  disruptor::dsl::Disruptor<ObjectBox> disruptor(
      ObjectBox::FACTORY, kRingSize, tf, disruptor::dsl::ProducerType::MULTI, ws);

  Consumer c1;
  Consumer c2;
  disruptor.handleEventsWith(c1).then(c2);

  auto& ringBuffer = disruptor.getRingBuffer();
  Publisher p;
  IMessage message;
  ITransportable transportable;
  std::string streamName = "com.lmax.wibble";

  std::cout << "publishing " << kRingSize << " messages\n";
  for (int i = 0; i < kRingSize; ++i) {
    ringBuffer.publishEvent(p, &message, &transportable, streamName);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  std::cout << "start disruptor\n";
  disruptor.start();
  std::cout << "continue publishing\n";

  for (int i = 0; i < 200; ++i) {
    ringBuffer.publishEvent(p, &message, &transportable, streamName);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  disruptor.shutdown(2000);
  return 0;
}


