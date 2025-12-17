#pragma once
// 1:1 port skeleton of com.lmax.disruptor.AlertException

#include <exception>

namespace disruptor {

class AlertException final : public std::exception {
public:
  const char* what() const noexcept override { return "AlertException"; }

  // Java: public static final AlertException INSTANCE = new AlertException();
  static const AlertException& INSTANCE() {
    static AlertException instance;
    return instance;
  }
};

} // namespace disruptor


