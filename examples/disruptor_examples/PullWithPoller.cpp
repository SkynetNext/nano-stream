// 1:1 port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/PullWithPoller.java

#include "disruptor/EventFactory.h"
#include "disruptor/EventPoller.h"
#include "disruptor/RingBuffer.h"

#include <cstdint>
#include <memory>

namespace {

template <typename T>
struct DataEvent {
  T data{};

  static std::shared_ptr<disruptor::EventFactory<DataEvent<T>>> factory() {
    struct F final : public disruptor::EventFactory<DataEvent<T>> {
      DataEvent<T> newInstance() override { return DataEvent<T>(); }
    };
    return std::make_shared<F>();
  }

  T copyOfData() { return data; }
};

template <typename T>
class OneAtATimeHandler final : public disruptor::EventPoller<DataEvent<T>>::Handler {
public:
  explicit OneAtATimeHandler(T* out) : out_(out) {}
  bool onEvent(DataEvent<T>& event, int64_t /*sequence*/, bool /*endOfBatch*/) override {
    *out_ = event.copyOfData();
    return false; // only one event at a time
  }
private:
  T* out_;
};

template <typename T>
static T getNextValue(disruptor::EventPoller<DataEvent<T>>& poller) {
  T out{};
  OneAtATimeHandler<T> h(&out);
  poller.poll(h);
  return out;
}

} // namespace

int main() {
  auto ringBuffer = disruptor::RingBuffer<DataEvent<void*>>::createMultiProducer(DataEvent<void*>::factory(), 1024);
  auto poller = ringBuffer->newPoller();

  void* value = getNextValue<void*>(*poller);
  (void)value;
  // Value could be null if no events are available.
  return 0;
}


