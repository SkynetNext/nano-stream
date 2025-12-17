// 1:1 port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/SequentialThreeConsumers.java

#include "disruptor/dsl/Disruptor.h"
#include "disruptor/util/DaemonThreadFactory.h"

#include <cstdint>
#include <memory>

namespace {
struct MyEvent {
  int64_t a{0};
  int64_t b{0};
  int64_t c{0};
  int64_t d{0};
};

struct MyEventFactory final : public disruptor::EventFactory<MyEvent> {
  MyEvent newInstance() override { return MyEvent(); }
};

class CopyAtoB final : public disruptor::EventHandler<MyEvent> {
public:
  void onEvent(MyEvent& event, int64_t /*sequence*/, bool /*endOfBatch*/) override { event.b = event.a; }
};
class CopyBtoC final : public disruptor::EventHandler<MyEvent> {
public:
  void onEvent(MyEvent& event, int64_t /*sequence*/, bool /*endOfBatch*/) override { event.c = event.b; }
};
class CopyCtoD final : public disruptor::EventHandler<MyEvent> {
public:
  void onEvent(MyEvent& event, int64_t /*sequence*/, bool /*endOfBatch*/) override { event.d = event.c; }
};
} // namespace

int main() {
  auto& tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  auto factory = std::make_shared<MyEventFactory>();
  disruptor::dsl::Disruptor<MyEvent> disruptor(factory, 1024, tf);

  CopyAtoB h1;
  CopyBtoC h2;
  CopyCtoD h3;
  disruptor.handleEventsWith(h1).then(h2).then(h3);
  disruptor.start();
  disruptor.shutdown();
  return 0;
}


