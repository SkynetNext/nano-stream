#pragma once
// 1:1 port of com.lmax.disruptor.IgnoreExceptionHandler
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/IgnoreExceptionHandler.java

#include "ExceptionHandler.h"

#include <cstdint>
#include <exception>
#include <iostream>

namespace disruptor {

template <typename T>
class IgnoreExceptionHandler final : public ExceptionHandler<T> {
public:
  void handleEventException(const std::exception& ex, int64_t sequence, T* event) override {
    std::cerr << "Exception processing: " << sequence << " " << event << " : " << ex.what() << "\n";
  }

  void handleOnStartException(const std::exception& ex) override {
    std::cerr << "Exception during onStart(): " << ex.what() << "\n";
  }

  void handleOnShutdownException(const std::exception& ex) override {
    std::cerr << "Exception during onShutdown(): " << ex.what() << "\n";
  }
};

} // namespace disruptor
