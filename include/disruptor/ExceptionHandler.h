#pragma once
// 1:1 port of com.lmax.disruptor.ExceptionHandler
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/ExceptionHandler.java

#include <cstdint>
#include <exception>

namespace disruptor {

template <typename T>
class ExceptionHandler {
public:
  virtual ~ExceptionHandler() = default;

  // Java allows null event (Object). C++ port models this as nullable pointer.
  virtual void handleEventException(const std::exception& ex, int64_t sequence, T* event) = 0;
  virtual void handleOnStartException(const std::exception& ex) = 0;
  virtual void handleOnShutdownException(const std::exception& ex) = 0;
};

} // namespace disruptor
