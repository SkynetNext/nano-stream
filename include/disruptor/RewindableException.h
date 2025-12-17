#pragma once
// 1:1 port of com.lmax.disruptor.RewindableException
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/RewindableException.java

#include <exception>
#include <string>

namespace disruptor {

class RewindableException : public std::exception {
public:
  explicit RewindableException(std::string causeWhat)
      : message_("REWINDING BATCH"), causeWhat_(std::move(causeWhat)) {}

  const char* what() const noexcept override { return message_.c_str(); }
  const std::string& causeWhat() const noexcept { return causeWhat_; }

private:
  std::string message_;
  std::string causeWhat_;
};

} // namespace disruptor
