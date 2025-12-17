// 1:1 port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/PullWithPoller.java

#include "disruptor/EventFactory.h"
#include "disruptor/EventPoller.h"
#include "disruptor/RingBuffer.h"

#include <cstdint>
#include <memory>

namespace {

template <typename T> struct DataEvent {
  T data{};

  static std::shared_ptr<disruptor::EventFactory<DataEvent<T>>> factory() {
    struct F final : public disruptor::EventFactory<DataEvent<T>> {
      DataEvent<T> newInstance() override { return DataEvent<T>(); }
    };
    return std::make_shared<F>();
  }

  T copyOfData() { return data; }
};

template <typename T, typename PollerT>
class OneAtATimeHandler final : public PollerT::Handler {
public:
  explicit OneAtATimeHandler(T *out) : out_(out) {}
  bool onEvent(DataEvent<T> &event, int64_t /*sequence*/,
               bool /*endOfBatch*/) override {
    *out_ = event.copyOfData();
    return false; // only one event at a time
  }

private:
  T *out_;
};

template <typename T, typename PollerT> static T getNextValue(PollerT &poller) {
  T out{};
  OneAtATimeHandler<T, PollerT> h(&out);
  poller.poll(h);
  return out;
}

} // namespace

int main() {
  disruptor::BlockingWaitStrategy ws;
  using Seq =
      disruptor::MultiProducerSequencer<disruptor::BlockingWaitStrategy>;
  using RB = disruptor::RingBuffer<DataEvent<void *>, Seq>;
  auto ringBuffer =
      RB::createMultiProducer(DataEvent<void *>::factory(), 1024, ws);
  auto poller = ringBuffer->newPoller();

  void *value = getNextValue<void *>(*poller);
  (void)value;
  // Value could be null if no events are available.
  return 0;
}
