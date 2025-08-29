#include "aeron.h"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
  constexpr size_t BUFFER_SIZE = 1024;

  std::cout << "Aeron IPC Example (using Simple Aeron) - Version "
            << aeron::Version::get_version_string() << std::endl;

  try {
    // Create publication
    std::cout << "Creating publication..." << std::endl;
    auto publication =
        aeron_simple::SimpleAeron::create_publication<SimpleEvent>(BUFFER_SIZE);

    // Create subscription in another thread
    std::atomic<bool> stop_consumer{false};
    std::atomic<int> events_received{0};

    std::thread consumer_thread([&]() {
      std::cout << "Creating subscription..." << std::endl;

      // Wait a bit for publication to be ready
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      auto subscription =
          aeron_simple::SimpleAeron::create_subscription(*publication);

      std::cout << "Starting to poll for events..." << std::endl;

      while (!stop_consumer.load()) {
        int processed = subscription->poll(
            [&](const SimpleEvent &event, int64_t sequence, bool end_of_batch) {
              events_received.fetch_add(1);
              std::cout << "Received event: id=" << event.id
                        << ", value=" << event.value << ", message='"
                        << event.message << "'"
                        << ", sequence=" << sequence << std::endl;
            },
            10);

        if (processed == 0) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      }
    });

    // Give consumer time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Publish some events
    std::cout << "Publishing events..." << std::endl;

    for (int i = 1; i <= 10; ++i) {
      SimpleEvent event(i, i * 3.14, "Event " + std::to_string(i));

      // Try to publish with retry
      int retry_count = 0;
      while (!publication->offer(event) && retry_count < 1000) {
        std::this_thread::sleep_for(std::chrono::microseconds(1));
        ++retry_count;
      }

      if (retry_count >= 1000) {
        std::cerr << "Failed to publish event " << i << " after retries"
                  << std::endl;
      } else {
        std::cout << "Published event " << i << std::endl;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Wait for all events to be consumed
    std::cout << "Waiting for events to be consumed..." << std::endl;
    int wait_count = 0;
    while (events_received.load() < 10 && wait_count < 50) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      ++wait_count;
    }

    // Stop consumer
    stop_consumer.store(true);
    consumer_thread.join();

    std::cout << "Events received: " << events_received.load() << "/10"
              << std::endl;
    std::cout << "Remaining capacity: " << publication->remaining_capacity()
              << std::endl;

    std::cout << "Example completed successfully!" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}