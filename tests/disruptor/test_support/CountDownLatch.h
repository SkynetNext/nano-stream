#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace disruptor::test_support {

class CountDownLatch {
public:
  explicit CountDownLatch(int64_t count) : count_(count) {}

  void countDown() {
    std::lock_guard<std::mutex> lock(mu_);
    if (count_ > 0) {
      --count_;
    }
    if (count_ == 0) {
      cv_.notify_all();
    }
  }

  void await() {
    std::unique_lock<std::mutex> lock(mu_);
    cv_.wait(lock, [&] { return count_ == 0; });
  }

private:
  std::mutex mu_;
  std::condition_variable cv_;
  int64_t count_;
};

} // namespace disruptor::test_support


