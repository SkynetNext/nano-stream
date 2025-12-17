#include <benchmark/benchmark.h>

#include "jmh_config.h"

#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

#if defined(__linux__)
#include <sys/uio.h>
#include "tbus.h" // reference/tsf4g/tbus/include/tbus.h (wired via CMake on Linux)
#endif

static void JMH_TBusSingleProducerSingleConsumer_producing(benchmark::State& state) {
#if !defined(__linux__)
  state.SkipWithError("TSF4G tbus benchmark requires Linux (SysV headers / iovec).");
  return;
#else
  // Strictly use the reference TSF4G tbus implementation and data layout.
  // We allocate a local tbus_t blob (no SysV shm) but use the original API:
  //   tbus_init / tbus_send_begin/end / tbus_read_begin/end
  constexpr size_t kPayloadBytes = sizeof(uint64_t);
  constexpr size_t kSlots = 1u << 20;

  const size_t total_size = tbus_size(kPayloadBytes, kSlots);
  std::vector<uint8_t> storage(total_size);
  auto* tb = reinterpret_cast<tbus_t*>(storage.data());
  tbus_init(tb, kPayloadBytes, kSlots);

  volatile bool running = true;

  std::thread consumer([&] {
    uint64_t v = 0;
    struct iovec iov[8];
    while (running) {
      size_t iov_num = 8;
      tbus_atomic_size_t head = tbus_read_begin(tb, iov, &iov_num);
      if (iov_num == 0) {
        std::this_thread::yield();
        continue;
      }
      // Consume first segment (SPSC benchmark uses fixed-size payload).
      std::memcpy(&v, iov[0].iov_base, (std::min<size_t>)(iov[0].iov_len, sizeof(uint64_t)));
      benchmark::DoNotOptimize(v);
      tbus_read_end(tb, head);
    }
  });

  uint64_t payload = 0;
  for (auto _ : state) {
    char* buf = nullptr;
    while (tbus_send_begin(tb, &buf) == 0) {
      std::this_thread::yield();
    }
    std::memcpy(buf, &payload, sizeof(payload));
    tbus_send_end(tb, static_cast<tbus_atomic_size_t>(sizeof(payload)));
    ++payload;
  }

  running = false;
  consumer.join();
#endif
}

static auto* bm_JMH_TBusSingleProducerSingleConsumer_producing = [] {
  auto* b = benchmark::RegisterBenchmark("JMH_TBusSingleProducerSingleConsumer_producing",
                                         &JMH_TBusSingleProducerSingleConsumer_producing);
  return nano_stream::bench::jmh::applyJmhDefaults(b);
}();


