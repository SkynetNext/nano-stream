#pragma once
// 1:1 port of com.lmax.disruptor.support.StubEvent (test helper)

#include "disruptor/EventFactory.h"
#include "disruptor/EventTranslatorTwoArg.h"

#include <cstdint>
#include <string>

namespace disruptor::support {

class StubEvent {
public:
  int value{-1};
  std::string testString{};

  explicit StubEvent(int i) : value(i) {}
  StubEvent() = default;

  void copy(const StubEvent& event) { value = event.value; }

  int getValue() const { return value; }
  void setValue(int v) { value = v; }

  const std::string& getTestString() const { return testString; }
  void setTestString(std::string s) { testString = std::move(s); }

  struct Translator final : public ::disruptor::EventTranslatorTwoArg<StubEvent, int, std::string> {
    void translateTo(StubEvent& event, int64_t /*sequence*/, int arg0, std::string arg1) override {
      event.setValue(arg0);
      event.setTestString(std::move(arg1));
    }
  };

  static inline Translator TRANSLATOR{};

  struct Factory final : public ::disruptor::EventFactory<StubEvent> {
    StubEvent newInstance() override { return StubEvent(-1); }
  };

  static inline std::shared_ptr<::disruptor::EventFactory<StubEvent>> EVENT_FACTORY = std::make_shared<Factory>();

  bool operator==(const StubEvent& other) const { return value == other.value && testString == other.testString; }
};

} // namespace disruptor::support


