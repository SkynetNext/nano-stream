#pragma once
// 1:1 port of com.lmax.disruptor.dsl.stubs.EventHandlerStub
// Source: reference/disruptor/src/test/java/com/lmax/disruptor/dsl/stubs/EventHandlerStub.java

#include "disruptor/EventHandler.h"
#include "tests/disruptor/test_support/CountDownLatch.h"

namespace disruptor::dsl::stubs {

template <typename T>
class EventHandlerStub final : public disruptor::EventHandler<T> {
public:
  explicit EventHandlerStub(disruptor::test_support::CountDownLatch& countDownLatch)
      : countDownLatch_(&countDownLatch) {}

  void onEvent(T& /*entry*/, int64_t /*sequence*/, bool /*endOfBatch*/) override {
    countDownLatch_->countDown();
  }

private:
  disruptor::test_support::CountDownLatch* countDownLatch_;
};

} // namespace disruptor::dsl::stubs

