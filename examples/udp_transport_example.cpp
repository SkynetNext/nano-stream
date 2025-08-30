#include "aeron/client/aeron.h"
#include "aeron/driver/media_driver.h"
#include "aeron/logbuffer/frame_descriptor.h"
#include "aeron/logbuffer/log_buffers.h"
#include "aeron/util/path_utils.h"
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

int main() {
  std::cout << "=== Aeron UDP Transport Example ===" << std::endl;

  try {
    // 1. Start Media Driver
    std::cout << "1. Starting Media Driver..." << std::endl;
    auto media_driver = aeron::driver::MediaDriver::launch();
    media_driver->start();

    // Wait a moment for driver to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Check if ports are available (basic check)
    std::cout << "Checking UDP ports..." << std::endl;
    std::cout << "UDP endpoint: localhost:40456" << std::endl;
    std::cout << "UDP control: localhost:40457" << std::endl;

    // 2. Create Aeron client
    std::cout << "2. Creating Aeron client..." << std::endl;
    aeron::client::AeronContext context;
    auto aeron = std::make_unique<aeron::client::Aeron>(context);

    // 3. Create publication
    std::cout << "3. Creating publication..." << std::endl;
    std::string channel =
        "aeron:udp?endpoint=localhost:40456|control=localhost:40457";
    std::int32_t stream_id = 1001;

    auto publication = aeron->add_publication(channel, stream_id);
    if (!publication) {
      std::cerr << "Failed to create publication" << std::endl;
      return 1;
    }

    std::cout << "Publication created: channel=" << channel
              << ", stream=" << stream_id << std::endl;

    // Wait for publication to be ready
    std::cout << "Waiting for publication to be ready..." << std::endl;
    while (!publication->is_connected()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    std::cout << "Publication is ready!" << std::endl;

    // 4. Create subscription
    std::cout << "4. Creating subscription..." << std::endl;
    auto subscription = aeron->add_subscription(channel, stream_id);
    if (!subscription) {
      std::cerr << "Failed to create subscription" << std::endl;
      return 1;
    }

    std::cout << "Subscription created: channel=" << channel
              << ", stream=" << stream_id << std::endl;

    // Wait for subscription to be ready
    std::cout << "Waiting for subscription to be ready..." << std::endl;
    int wait_count = 0;
    while (!subscription->is_connected()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      wait_count++;
      if (wait_count > 1000) { // 10 seconds timeout
        std::cout << "Warning: Subscription not ready after 10 seconds"
                  << std::endl;
        break;
      }
    }
    std::cout << "Subscription is ready! (waited " << wait_count * 10 << "ms)"
              << std::endl;

    // 5. Send messages
    std::cout << "5. Sending messages..." << std::endl;

    for (int i = 0; i < 5; ++i) {
      std::string message = "Hello Aeron UDP " + std::to_string(i);

      auto result = publication->offer(
          reinterpret_cast<const std::uint8_t *>(message.c_str()), 0,
          message.length());

      if (result == aeron::client::PublicationResult::SUCCESS) {
        std::cout << "Sent message " << i << ": " << message << " (SUCCESS)"
                  << std::endl;
      } else {
        std::cout << "Failed to send message " << i
                  << ", result: " << static_cast<int>(result) << std::endl;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Wait a bit more for messages to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Debug: Check subscription status
    std::cout << "Debug: Subscription connected: "
              << (subscription->is_connected() ? "YES" : "NO") << std::endl;
    std::cout << "Debug: Publication connected: "
              << (publication->is_connected() ? "YES" : "NO") << std::endl;

    // 6. Receive messages
    std::cout << "6. Receiving messages..." << std::endl;

    int messages_received = 0;
    auto start_time = std::chrono::steady_clock::now();

    while (messages_received < 5) {
      auto fragments_read = subscription->poll(
          [&](const std::uint8_t *buffer, std::int32_t offset,
              std::int32_t length, const void *header) {
            std::string received_message(
                reinterpret_cast<const char *>(buffer + offset), length);
            std::cout << "Received message: " << received_message << std::endl;
            messages_received++;
          },
          10);

      if (fragments_read == 0) {
        // Debug: Print poll attempts
        static int poll_attempts = 0;
        poll_attempts++;
        if (poll_attempts % 20 == 0) { // Print every 20 attempts
          std::cout << "Debug: Poll attempt " << poll_attempts
                    << ", fragments_read=0" << std::endl;
        }
        // Check timeout
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - start_time)
                .count() > 10) { // Increased timeout
          std::cout << "Timeout waiting for messages" << std::endl;
          break;
        }
        std::this_thread::sleep_for(
            std::chrono::milliseconds(50)); // Increased sleep
      }
    }

    std::cout << "Received " << messages_received << " messages" << std::endl;

    // 7. Cleanup
    std::cout << "7. Cleaning up..." << std::endl;

    publication->close();
    subscription->close();
    aeron->close();

    media_driver->stop();

    std::cout << "UDP transport example completed successfully" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
