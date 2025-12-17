#pragma once
// 1:1 port of com.lmax.disruptor.ExceptionHandlers
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/ExceptionHandlers.java

#include "ExceptionHandler.h"
#include "FatalExceptionHandler.h"

#include <memory>

namespace disruptor {

class ExceptionHandlers final {
public:
  ExceptionHandlers() = delete;

  // Java: public static ExceptionHandler<Object> defaultHandler()
  template <typename T>
  static std::shared_ptr<ExceptionHandler<T>> defaultHandler() {
    // Java holder idiom -> function-local static.
    static std::shared_ptr<ExceptionHandler<T>> handler = std::make_shared<FatalExceptionHandler<T>>();
    return handler;
  }
};

} // namespace disruptor
