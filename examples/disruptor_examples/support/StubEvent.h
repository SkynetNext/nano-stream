#pragma once
// 1:1 port of:
// reference/disruptor/src/examples/java/com/lmax/disruptor/examples/support/StubEvent.java

#include "disruptor/EventFactory.h"
#include "disruptor/EventTranslatorTwoArg.h"

#include <cstdint>
#include <memory>
#include <string>

namespace disruptor_examples::support {

class StubEvent final {
public:
  StubEvent() = default;
  explicit StubEvent(int value) : value_(value) {}

  void copy(const StubEvent& event) { value_ = event.value_; }

  int getValue() const { return value_; }
  void setValue(int value) { value_ = value; }

  const std::string& getTestString() const { return testString_; }
  void setTestString(std::string testString) { testString_ = std::move(testString); }

  bool operator==(const StubEvent& other) const {
    return value_ == other.value_ && testString_ == other.testString_;
  }

  struct Factory final : public disruptor::EventFactory<StubEvent> {
    StubEvent newInstance() override { return StubEvent(-1); }
  };

  static inline std::shared_ptr<disruptor::EventFactory<StubEvent>> EVENT_FACTORY = std::make_shared<Factory>();

  struct Translator final : public disruptor::EventTranslatorTwoArg<StubEvent, int, std::string> {
    void translateTo(StubEvent& event, int64_t /*sequence*/, int arg0, std::string arg1) override {
      event.setValue(arg0);
      event.setTestString(std::move(arg1));
    }
  };

  static inline Translator TRANSLATOR{};

private:
  int value_{-1};
  std::string testString_{};
};

} // namespace disruptor_examples::support


