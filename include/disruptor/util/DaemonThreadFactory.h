#pragma once
// 1:1 port of com.lmax.disruptor.util.DaemonThreadFactory
// Source: reference/disruptor/src/main/java/com/lmax/disruptor/util/DaemonThreadFactory.java

#include "../dsl/ThreadFactory.h"

#include <functional>
#include <thread>

namespace disruptor::util {

// Java is an enum singleton: INSTANCE.
class DaemonThreadFactory final : public disruptor::dsl::ThreadFactory {
public:
  static DaemonThreadFactory& INSTANCE() {
    static DaemonThreadFactory instance;
    return instance;
  }

  std::thread newThread(std::function<void()> r) override {
    // Java sets daemon=true. In C++ we can't mark a thread as daemon, so we return a joinable
    // std::thread and let the caller decide whether to detach (DSL does).
    return std::thread(std::move(r));
  }

private:
  DaemonThreadFactory() = default;
};

} // namespace disruptor::util
