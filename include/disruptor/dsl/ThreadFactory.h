#pragma once
// C++ adapter for java.util.concurrent.ThreadFactory used by com.lmax.disruptor.dsl.*
//
// Java source reference uses java.util.concurrent.ThreadFactory:
// reference/disruptor/src/main/java/com/lmax/disruptor/dsl/*.java
//
// This is intentionally minimal: it exists to keep the DSL 1:1 while modeling threads in C++.

#include <functional>
#include <thread>

namespace disruptor::dsl {

class ThreadFactory {
public:
  virtual ~ThreadFactory() = default;

  // Java: Thread newThread(Runnable r)
  // C++: return a std::thread that runs r (it starts immediately).
  virtual std::thread newThread(std::function<void()> r) = 0;
};

} // namespace disruptor::dsl


