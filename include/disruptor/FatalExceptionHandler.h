#pragma once
// 1:1 port of com.lmax.disruptor.FatalExceptionHandler
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/FatalExceptionHandler.java

#include "ExceptionHandler.h"

#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace disruptor {

// Java implements ExceptionHandler<Object>. Here we template for symmetry with C++ API.
template <typename T>
class FatalExceptionHandler final : public ExceptionHandler<T> {
public:
  void handleEventException(const std::exception& ex, int64_t sequence, T* event) override {
    // Java: log ERROR and throw new RuntimeException(ex)
    std::cerr << "Exception processing: " << sequence << " " << event << " : " << ex.what() << "\n";
    throw std::runtime_error(ex.what());
  }

  void handleOnStartException(const std::exception& ex) override {
    std::cerr << "Exception during onStart(): " << ex.what() << "\n";
  }

  void handleOnShutdownException(const std::exception& ex) override {
    std::cerr << "Exception during onShutdown(): " << ex.what() << "\n";
  }
};

} // namespace disruptor
