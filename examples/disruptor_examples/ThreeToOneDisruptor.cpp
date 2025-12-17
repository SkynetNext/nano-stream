// 1:1 port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/ThreeToOneDisruptor.java

#include "disruptor/dsl/Disruptor.h"
#include "disruptor/util/DaemonThreadFactory.h"

#include <array>
#include <cstdint>

namespace {

struct DataEvent {
  int64_t input{0};
  std::array<int64_t, 3> output{};

  struct Factory final : public disruptor::EventFactory<DataEvent> {
    DataEvent newInstance() override { return DataEvent(); }
  };
  static inline std::shared_ptr<disruptor::EventFactory<DataEvent>> FACTORY = std::make_shared<Factory>();
};

class TransformingHandler final : public disruptor::EventHandler<DataEvent> {
public:
  explicit TransformingHandler(int outputIndex) : outputIndex_(outputIndex) {}

  void onEvent(DataEvent& event, int64_t /*sequence*/, bool /*endOfBatch*/) override {
    event.output[static_cast<size_t>(outputIndex_)] = doSomething(event.input);
  }

private:
  int outputIndex_;
  static int64_t doSomething(int64_t input) { return input; }
};

class CollatingHandler final : public disruptor::EventHandler<DataEvent> {
public:
  void onEvent(DataEvent& /*event*/, int64_t /*sequence*/, bool /*endOfBatch*/) override {
    // Do required collation here...
  }
};

} // namespace

int main() {
  auto& tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  disruptor::dsl::Disruptor<DataEvent> disruptor(DataEvent::FACTORY, 1024, tf);

  TransformingHandler handler1(0);
  TransformingHandler handler2(1);
  TransformingHandler handler3(2);
  CollatingHandler collator;

  disruptor.handleEventsWith(handler1, handler2, handler3).then(collator);
  disruptor.start();
  disruptor.shutdown();
  return 0;
}


