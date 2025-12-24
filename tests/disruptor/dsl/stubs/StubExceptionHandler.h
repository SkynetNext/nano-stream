#pragma once
// 1:1 port of com.lmax.disruptor.dsl.stubs.StubExceptionHandler
// Source: reference/disruptor/src/test/java/com/lmax/disruptor/dsl/stubs/StubExceptionHandler.java

#include "disruptor/ExceptionHandler.h"

#include <atomic>
#include <exception>
#include <stdexcept>

namespace disruptor::dsl::stubs {

template <typename T>
class StubExceptionHandler final : public disruptor::ExceptionHandler<T> {
public:
  explicit StubExceptionHandler(std::atomic<std::exception*>& exceptionHandled)
      : exceptionHandled_(&exceptionHandled) {}

  void handleEventException(const std::exception& ex, int64_t /*sequence*/,
                            T* /*event*/) override {
    exceptionHandled_->store(const_cast<std::exception*>(&ex));
  }

  void handleOnStartException(const std::exception& ex) override {
    exceptionHandled_->store(const_cast<std::exception*>(&ex));
  }

  void handleOnShutdownException(const std::exception& ex) override {
    exceptionHandled_->store(const_cast<std::exception*>(&ex));
  }

private:
  std::atomic<std::exception*>* exceptionHandled_;
};

} // namespace disruptor::dsl::stubs

