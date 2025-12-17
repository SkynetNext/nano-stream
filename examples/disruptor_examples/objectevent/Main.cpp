// 1:1-ish port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/objectevent/Main.java

#include "disruptor/dsl/Disruptor.h"
#include "disruptor/BlockingWaitStrategy.h"
#include "disruptor/util/DaemonThreadFactory.h"

#include <cstdint>
#include <memory>
#include <string>

namespace {

template <typename T>
struct ObjectEvent {
  T val{};
  void clear() { val = T{}; }
};

template <typename T>
class ProcessingEventHandler final : public disruptor::EventHandler<ObjectEvent<T>> {
public:
  void onEvent(ObjectEvent<T>& /*event*/, int64_t /*sequence*/, bool /*endOfBatch*/) override {}
};

template <typename T>
class ClearingEventHandler final : public disruptor::EventHandler<ObjectEvent<T>> {
public:
  void onEvent(ObjectEvent<T>& event, int64_t /*sequence*/, bool /*endOfBatch*/) override { event.clear(); }
};

struct Factory final : public disruptor::EventFactory<ObjectEvent<std::string>> {
  ObjectEvent<std::string> newInstance() override { return ObjectEvent<std::string>(); }
};

} // namespace

int main() {
  constexpr int bufferSize = 1024;
  auto& tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  auto factory = std::make_shared<Factory>();
  using Event = ObjectEvent<std::string>;
  using WS = disruptor::BlockingWaitStrategy;
  constexpr auto Producer = disruptor::dsl::ProducerType::MULTI;
  using DisruptorT = disruptor::dsl::Disruptor<Event, Producer, WS>;

  WS ws;
  DisruptorT disruptor(factory, bufferSize, tf, ws);

  ProcessingEventHandler<std::string> processing;
  ClearingEventHandler<std::string> clearing;
  disruptor.handleEventsWith(processing).then(clearing);
  disruptor.start();
  disruptor.shutdown();
  return 0;
}


