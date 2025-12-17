#pragma once
// 1:1 port of com.lmax.disruptor.TimeoutException
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/TimeoutException.java

#include <exception>

namespace disruptor {
class TimeoutException final : public std::exception {
public:
  const char* what() const noexcept override { return "TimeoutException"; }

  // Java: public static final TimeoutException INSTANCE = new TimeoutException();
  static const TimeoutException& INSTANCE() {
    static TimeoutException instance;
    return instance;
  }

private:
  TimeoutException() = default;
};
} // namespace disruptor


