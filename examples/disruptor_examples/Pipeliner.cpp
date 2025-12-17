// 1:1 port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/Pipeliner.java

#include "disruptor/EventFactory.h"
#include "disruptor/RingBuffer.h"
#include "disruptor/dsl/Disruptor.h"
#include "disruptor/util/DaemonThreadFactory.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

namespace {

struct PipelinerEvent {
  int64_t input{0};
  std::optional<std::string> result{};

  struct Factory final : public disruptor::EventFactory<PipelinerEvent> {
    PipelinerEvent newInstance() override { return PipelinerEvent(); }
  };
  static inline std::shared_ptr<disruptor::EventFactory<PipelinerEvent>> FACTORY = std::make_shared<Factory>();
};

class ParallelHandler final : public disruptor::EventHandler<PipelinerEvent> {
public:
  ParallelHandler(int ordinal, int totalHandlers) : ordinal_(ordinal), totalHandlers_(totalHandlers) {}

  void onEvent(PipelinerEvent& event, int64_t sequence, bool /*endOfBatch*/) override {
    if ((sequence % totalHandlers_) == ordinal_) {
      event.result = std::to_string(event.input);
    }
  }

private:
  int ordinal_;
  int totalHandlers_;
};

class JoiningHandler final : public disruptor::EventHandler<PipelinerEvent> {
public:
  void onEvent(PipelinerEvent& event, int64_t /*sequence*/, bool /*endOfBatch*/) override {
    if (event.input != lastEvent_ + 1 || !event.result.has_value()) {
      std::cout << "Error: input=" << event.input << " last=" << lastEvent_ << " result="
                << (event.result ? *event.result : std::string("null")) << "\n";
      ++errors_;
    }
    lastEvent_ = event.input;
    event.result.reset();
  }

  int errors() const { return errors_; }

private:
  int64_t lastEvent_{-1};
  int errors_{0};
};

} // namespace

int main() {
  auto& tf = disruptor::util::DaemonThreadFactory::INSTANCE();
  disruptor::dsl::Disruptor<PipelinerEvent> disruptor(PipelinerEvent::FACTORY, 1024, tf);

  ParallelHandler h0(0, 3);
  ParallelHandler h1(1, 3);
  ParallelHandler h2(2, 3);
  JoiningHandler join;

  disruptor.handleEventsWith(h0, h1, h2).then(join);
  auto rb = disruptor.start();

  for (int i = 0; i < 1000; ++i) {
    int64_t next = rb->next();
    try {
      auto& e = rb->get(next);
      e.input = i;
    } catch (...) {
      rb->publish(next);
      throw;
    }
    rb->publish(next);
  }

  disruptor.shutdown(2000);
  return join.errors() == 0 ? 0 : 1;
}


