#include "disruptor.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace disruptor;

// Event structure
struct TradeEvent {
  int64_t order_id;
  double price;
  int quantity;
  std::string symbol;

  TradeEvent() : order_id(0), price(0.0), quantity(0) {}
};

// Event handlers
class JournalHandler : public EventHandler<TradeEvent> {
public:
  void on_event(TradeEvent &event, int64_t sequence,
                bool end_of_batch) override {
    std::cout << "[Journal] Writing trade " << event.order_id
              << " to persistent log" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Simulate I/O
  }
};

class ReplicationHandler : public EventHandler<TradeEvent> {
public:
  void on_event(TradeEvent &event, int64_t sequence,
                bool end_of_batch) override {
    std::cout << "[Replication] Sending trade " << event.order_id
              << " to backup server" << std::endl;
    std::this_thread::sleep_for(
        std::chrono::milliseconds(15)); // Simulate network
  }
};

class BusinessLogicHandler : public EventHandler<TradeEvent> {
public:
  void on_event(TradeEvent &event, int64_t sequence,
                bool end_of_batch) override {
    std::cout << "[Business] Processing trade " << event.order_id << " - "
              << event.quantity << " " << event.symbol << " @ " << event.price
              << std::endl;
    std::this_thread::sleep_for(
        std::chrono::milliseconds(5)); // Simulate processing
  }
};

int main() {
  std::cout << "=== Disruptor SPMC Example ===" << std::endl;
  std::cout << "Demonstrating Single Producer, Multiple Consumer with "
               "dependency graph"
            << std::endl;
  std::cout << std::endl;

  try {
    // Create ring buffer
    auto ring_buffer = RingBuffer<TradeEvent>::createSingleProducer(
        1024, []() { return TradeEvent(); });

    // Create Disruptor with DSL
    Disruptor<TradeEvent> disruptor(ring_buffer);

    // Set up consumer dependency graph:
    // Journal and Replication run in parallel
    // Business Logic waits for both to complete
    disruptor
        .handle_events_with(std::make_unique<JournalHandler>(),
                            std::make_unique<ReplicationHandler>())
        .then(std::make_unique<BusinessLogicHandler>());

    // Start the Disruptor
    std::cout << "Starting Disruptor..." << std::endl;
    disruptor.start();

    // Give processors time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Publish events
    std::cout << "Publishing trade events..." << std::endl;

    for (int i = 1; i <= 5; ++i) {
      int64_t sequence = ring_buffer.next();

      TradeEvent &event = ring_buffer.get(sequence);
      event.order_id = i;
      event.price = 100.0 + i * 0.5;
      event.quantity = i * 100;
      event.symbol = "AAPL";

      ring_buffer.publish(sequence);

      std::cout << "Published trade " << i << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    // Wait for all events to be processed
    std::cout << "Waiting for events to be processed..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // Stop the Disruptor
    std::cout << "Stopping Disruptor..." << std::endl;
    disruptor.stop();
    std::cout << "Disruptor stopped successfully!" << std::endl;

    std::cout << "Example completed successfully!" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
