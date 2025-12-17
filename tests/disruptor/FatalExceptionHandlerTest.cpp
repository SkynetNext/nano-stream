#include <gtest/gtest.h>

#include "disruptor/FatalExceptionHandler.h"
#include "tests/disruptor/support/TestEvent.h"

#include <stdexcept>

TEST(FatalExceptionHandlerTest, shouldHandleFatalException) {
  const std::runtime_error causeException("cause");
  disruptor::support::TestEvent event;

  disruptor::FatalExceptionHandler<disruptor::support::TestEvent> exceptionHandler;

  try {
    exceptionHandler.handleEventException(causeException, 0, &event);
    FAIL() << "Expected exception";
  } catch (const std::runtime_error& ex) {
    EXPECT_STREQ(ex.what(), causeException.what());
  }
}
