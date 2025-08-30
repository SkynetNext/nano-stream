#include "disruptor.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <thread>

using namespace disruptor;

// Example event structure
struct TradeEvent {
  int64_t trade_id = 0;
  double price = 0.0;
  int64_t quantity = 0;
  int64_t timestamp = 0;

  TradeEvent() = default;
  TradeEvent(int64_t id, double p, int64_t q, int64_t ts)
      : trade_id(id), price(p), quantity(q), timestamp(ts) {}
};

int main() {
  std::cout << "Nano-Stream Library Example\n";
  std::cout << "Version: " << Version::get_version_string() << "\n\n";

  // Create a ring buffer for trade events
  const size_t BUFFER_SIZE = 1024;
  RingBuffer<TradeEvent> ring_buffer(BUFFER_SIZE,
                                     []() { return TradeEvent(); });

  // Consumer sequence for tracking consumer progress
  Sequence consumer_sequence;
  ring_buffer.add_gating_sequences({std::cref(consumer_sequence)});

  const int NUM_TRADES = 10000;
  std::atomic<bool> consumer_done{false};
  std::atomic<int> trades_processed{0};

  // Consumer thread
  std::thread consumer([&]() {
    int64_t next_to_read = 0;

    std::cout << "Consumer started...\n";

    while (trades_processed.load() < NUM_TRADES) {
      if (ring_buffer.is_available(next_to_read)) {
        const TradeEvent &trade = ring_buffer.get(next_to_read);

        // Process the trade (simulate some work)
        if (trade.trade_id % 1000 == 0) {
          std::cout << "Processed trade " << trade.trade_id
                    << " - Price: " << trade.price
                    << ", Quantity: " << trade.quantity << "\n";
        }

        // Update consumer sequence
        consumer_sequence.set(next_to_read);
        next_to_read++;
        trades_processed.fetch_add(1);
      } else {
        // No events available, yield
        std::this_thread::yield();
      }
    }

    consumer_done.store(true);
    std::cout << "Consumer finished processing " << trades_processed.load()
              << " trades\n";
  });

  // Producer (main thread)
  std::cout << "Producing " << NUM_TRADES << " trade events...\n";

  auto start_time = std::chrono::high_resolution_clock::now();

  for (int i = 0; i < NUM_TRADES; ++i) {
    // Claim next sequence
    int64_t sequence = ring_buffer.next();

    // Check for error
    if (sequence == -1) {
      std::cerr << "Error: Failed to claim sequence for trade " << i << "\n";
      continue;
    }

    // Get the pre-allocated event
    TradeEvent &trade = ring_buffer.get(sequence);

    // Fill in trade data
    trade.trade_id = i;
    trade.price = 100.0 + (i % 50) * 0.01; // Simulate price movement
    trade.quantity = 100 + (i % 10) * 10;  // Simulate quantity variation
    trade.timestamp =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();

    // Publish the event
    ring_buffer.publish(sequence);
  }

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
      end_time - start_time);

  std::cout << "Production completed in " << duration.count()
            << " microseconds\n";
  std::cout << "Average: " << (duration.count() * 1000.0 / NUM_TRADES)
            << " nanoseconds per event\n";

  // Wait for consumer to finish
  consumer.join();

  // Print some statistics
  std::cout << "\nRing Buffer Statistics:\n";
  std::cout << "Buffer size: " << ring_buffer.get_buffer_size() << "\n";
  std::cout << "Current cursor: " << ring_buffer.get_cursor() << "\n";
  std::cout << "Remaining capacity: " << ring_buffer.remaining_capacity()
            << "\n";

  std::cout << "\nExample completed successfully!\n";

  return 0;
}
