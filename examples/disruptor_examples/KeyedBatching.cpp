// 1:1-ish port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/KeyedBatching.java
//
// Java source defines only the handler type. This C++ port provides a small runnable main()
// that exercises the handler.

#include "disruptor/dsl/Disruptor.h"
#include "disruptor/util/DaemonThreadFactory.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {

struct KeyedEvent {
  int64_t key{0};
  std::string data{};
};

struct KeyedEventFactory final : public disruptor::EventFactory<KeyedEvent> {
  KeyedEvent newInstance() override { return KeyedEvent(); }
};

class KeyedBatching final : public disruptor::EventHandler<KeyedEvent> {
public:
  void onEvent(KeyedEvent& event, int64_t /*sequence*/, bool endOfBatch) override {
    if (!batch_.empty() && event.key != key_) {
      processBatch();
    }

    batch_.push_back(event.data);
    key_ = event.key;

    if (endOfBatch || batch_.size() >= MAX_BATCH_SIZE) {
      processBatch();
    }
  }

private:
  static constexpr size_t MAX_BATCH_SIZE = 100;
  std::vector<std::string> batch_{};
  int64_t key_{0};

  void processBatch() {
    // do work.
    batch_.clear();
  }
};

} // namespace

int main() {
  auto& tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  auto factory = std::make_shared<KeyedEventFactory>();
  disruptor::dsl::Disruptor<KeyedEvent> d(factory, 1024, tf);

  KeyedBatching handler;
  d.handleEventsWith(handler);
  auto rb = d.start();

  // Publish some keyed events.
  for (int i = 0; i < 10; ++i) {
    int64_t seq = rb->next();
    auto& e = rb->get(seq);
    e.key = i % 2;
    e.data = "item-" + std::to_string(i);
    rb->publish(seq);
  }

  d.shutdown(2000);
  return 0;
}


