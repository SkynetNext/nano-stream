#include <gtest/gtest.h>

#include "disruptor/IgnoreExceptionHandler.h"
#include "tests/disruptor/support/TestEvent.h"

#include <stdexcept>

TEST(IgnoreExceptionHandlerTest, shouldHandleAndIgnoreException) {
  const std::runtime_error ex("ex");
  disruptor::support::TestEvent event;

  disruptor::IgnoreExceptionHandler<disruptor::support::TestEvent> exceptionHandler;
  EXPECT_NO_THROW(exceptionHandler.handleEventException(ex, 0, &event));
}
