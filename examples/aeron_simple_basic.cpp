#include "aeron.h"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
  std::cout << aeron::Version::get_full_version_string() << std::endl;

  try {
    constexpr size_t BUFFER_SIZE = 1024;

    // Create publication
    std::cout << "Creating publication..." << std::endl;
    auto publication =
        aeron_simple::SimpleAeron::create_publication<SimpleEvent>(BUFFER_SIZE);

    // Create subscription
    std::cout << "Creating subscription..." << std::endl;
    auto subscription =
        aeron_simple::SimpleAeron::create_subscription(*publication);

    // Start consumer in another thread
    std::atomic<bool> stop_consumer{false};
    std::atomic<int> events_received{0};

    std::thread consumer_thread([&]() {
      std::cout << "Starting consumer thread..." << std::endl;

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
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Publish some events
    std::cout << "Publishing events..." << std::endl;

    for (int i = 1; i <= 5; ++i) {
      SimpleEvent event(i, i * 3.14159, "Test message " + std::to_string(i));

      if (publication->offer(event)) {
        std::cout << "Published event " << i << std::endl;
      } else {
        std::cerr << "Failed to publish event " << i << std::endl;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Wait for all events to be consumed
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Stop consumer
    stop_consumer.store(true);
    consumer_thread.join();

    std::cout << "\nFinal statistics:" << std::endl;
    std::cout << "Events received: " << events_received.load() << std::endl;
    std::cout << "Remaining capacity: " << publication->remaining_capacity()
              << std::endl;

    std::cout << "\nAeron Simple Example completed successfully!" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
