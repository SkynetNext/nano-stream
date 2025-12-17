#include <gtest/gtest.h>

#include "disruptor/EventTranslator.h"
#include "tests/disruptor/support/StubEvent.h"

namespace {
static constexpr const char* TEST_VALUE = "Wibble";

class ExampleEventTranslator final : public disruptor::EventTranslator<disruptor::support::StubEvent> {
public:
  explicit ExampleEventTranslator(std::string testValue) : testValue_(std::move(testValue)) {}

  void translateTo(disruptor::support::StubEvent& event, int64_t /*sequence*/) override {
    event.setTestString(testValue_);
  }

private:
  std::string testValue_;
};
} // namespace

TEST(EventTranslatorTest, shouldTranslateOtherDataIntoAnEvent) {
  auto event = disruptor::support::StubEvent::EVENT_FACTORY->newInstance();
  ExampleEventTranslator eventTranslator(TEST_VALUE);

  eventTranslator.translateTo(event, 0);

  EXPECT_EQ(TEST_VALUE, event.getTestString());
}
