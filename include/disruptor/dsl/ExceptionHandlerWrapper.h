#pragma once
// 1:1 port of com.lmax.disruptor.dsl.ExceptionHandlerWrapper
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/dsl/ExceptionHandlerWrapper.java

#include "../ExceptionHandler.h"
#include "../ExceptionHandlers.h"

namespace disruptor::dsl {

template <typename T>
class ExceptionHandlerWrapper final : public ExceptionHandler<T> {
public:
  void switchTo(ExceptionHandler<T>& exceptionHandler) { delegate_ = &exceptionHandler; }

  void handleEventException(const std::exception& ex, int64_t sequence, T* event) override {
    getExceptionHandler().handleEventException(ex, sequence, event);
  }

  void handleOnStartException(const std::exception& ex) override { getExceptionHandler().handleOnStartException(ex); }

  void handleOnShutdownException(const std::exception& ex) override { getExceptionHandler().handleOnShutdownException(ex); }

private:
  ExceptionHandler<T>* delegate_{nullptr};

  ExceptionHandler<T>& getExceptionHandler() {
    auto* handler = delegate_;
    return handler == nullptr ? *ExceptionHandlers::defaultHandler<T>() : *handler;
  }
};

} // namespace disruptor::dsl


