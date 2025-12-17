#include <benchmark/benchmark.h>

#include "jmh_config.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

namespace {

// A minimal TSF4G-tbus-inspired SPSC ring:
// - byte ring buffer
// - (cmd,size) header + payload
// - single producer, single consumer
//
// This is intentionally self-contained (no SysV shm) so it can run on Windows and CI.
class TBusSpsc {
public:
  enum Cmd : uint32_t { kPackage = 1, kIgnore = 2 };

  struct Header {
    uint32_t cmd;
    uint32_t size;
  };

  TBusSpsc(size_t payload_bytes, size_t slots)
      : packet_size_(static_cast<uint32_t>(sizeof(Header) + payload_bytes)),
        size_(static_cast<uint32_t>(packet_size_ * slots)),
        buf_(size_, 0) {}

  // Producer: reserve space for payload, returns pointer into ring (valid until commit()).
  // Returns nullptr if full.
  uint8_t* begin_send(uint32_t& offset_out) {
    const uint32_t head = head_.load(std::memory_order_acquire);
    uint32_t tail = tail_.load(std::memory_order_relaxed);

    if (head <= tail) {
      uint32_t write_size = size_ - tail;
      if (write_size < sizeof(Header)) {
        if (head != 0) {
          tail_.store(0, std::memory_order_relaxed);
          tail = 0;
          write_size = size_;
        } else {
          return nullptr;
        }
      }

      if (write_size < packet_size_) {
        // Mark remainder as ignored then wrap if possible.
        auto* h = reinterpret_cast<Header*>(&buf_[tail]);
        h->cmd = kIgnore;
        h->size = 0;
        std::atomic_thread_fence(std::memory_order_release);
        if (head != 0) {
          tail_.store(0, std::memory_order_relaxed);
          return begin_send(offset_out);
        }
        return nullptr;
      }

      offset_out = tail;
      return &buf_[tail + sizeof(Header)];
    }

    // head > tail
    uint32_t write_size = head - tail - 1;
    if (write_size < packet_size_) return nullptr;
    offset_out = tail;
    return &buf_[tail + sizeof(Header)];
  }

  void commit_send(uint32_t offset, uint32_t payload_len) {
    auto* h = reinterpret_cast<Header*>(&buf_[offset]);
    h->cmd = kPackage;
    h->size = payload_len;
    std::atomic_thread_fence(std::memory_order_release);

    uint32_t tail = offset + static_cast<uint32_t>(sizeof(Header) + payload_len);
    if ((tail >= size_) && (head_.load(std::memory_order_acquire) != 0)) {
      tail = 0;
    }
    tail_.store(tail, std::memory_order_release);
  }

  // Consumer: tries to read one packet, returns true if read.
  bool try_read(uint64_t& out) {
    uint32_t head = head_.load(std::memory_order_relaxed);
    const uint32_t tail = tail_.load(std::memory_order_acquire);
    if (head == tail) return false;

    auto* h = reinterpret_cast<const Header*>(&buf_[head]);
    if ((size_ - head) < sizeof(Header)) {
      // Not enough space for header, wrap.
      head_.store(0, std::memory_order_release);
      return false;
    }

    if (h->cmd == kIgnore) {
      head_.store(0, std::memory_order_release);
      return false;
    }
    if (h->cmd != kPackage) return false;

    const uint32_t sz = h->size;
    if ((size_ - head) < sizeof(Header) + sz) return false;

    uint64_t tmp = 0;
    std::memcpy(&tmp, &buf_[head + sizeof(Header)], (std::min<uint32_t>)(sz, sizeof(uint64_t)));
    out = tmp;

    head += static_cast<uint32_t>(sizeof(Header) + sz);
    if ((head >= size_) && (tail_.load(std::memory_order_acquire) != 0)) {
      head = 0;
    }
    head_.store(head, std::memory_order_release);
    return true;
  }

private:
  const uint32_t packet_size_;
  const uint32_t size_;
  std::vector<uint8_t> buf_;
  alignas(64) std::atomic<uint32_t> head_{0};
  alignas(64) std::atomic<uint32_t> tail_{0};
};

constexpr size_t kPayloadBytes = sizeof(uint64_t);
constexpr size_t kSlots = 1u << 20; // roughly comparable ring sizing scale

} // namespace

static void JMH_TBusSingleProducerSingleConsumer_producing(benchmark::State& state) {
  TBusSpsc tb(kPayloadBytes, kSlots);
  std::atomic<bool> running{true};

  std::thread consumer([&] {
    uint64_t v = 0;
    while (running.load(std::memory_order_acquire)) {
      if (tb.try_read(v)) {
        benchmark::DoNotOptimize(v);
      } else {
        std::this_thread::yield();
      }
    }
    // Drain a bit on shutdown to reduce false "full" backpressure.
    for (int i = 0; i < 1000; ++i) {
      if (!tb.try_read(v)) break;
      benchmark::DoNotOptimize(v);
    }
  });

  uint64_t payload = 0;
  for (auto _ : state) {
    uint32_t off = 0;
    uint8_t* p = nullptr;
    while ((p = tb.begin_send(off)) == nullptr) {
      std::this_thread::yield();
    }
    std::memcpy(p, &payload, sizeof(payload));
    tb.commit_send(off, static_cast<uint32_t>(sizeof(payload)));
    payload++;
  }

  running.store(false, std::memory_order_release);
  consumer.join();
}

static auto* bm_JMH_TBusSingleProducerSingleConsumer_producing = [] {
  auto* b = benchmark::RegisterBenchmark("JMH_TBusSingleProducerSingleConsumer_producing",
                                         &JMH_TBusSingleProducerSingleConsumer_producing);
  return nano_stream::bench::jmh::applyJmhDefaults(b);
}();


