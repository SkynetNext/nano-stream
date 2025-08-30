// Prevent winsock.h inclusion on Windows to avoid conflicts with winsock2.h
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_ // Prevent winsock.h from being included
#endif
#endif

#include "aeron.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace aeron;

int main() {
  std::cout << "=== Aeron C++ Complete Example ===" << std::endl;
  std::cout << "Version: " << Version::get_full_version_string() << std::endl;
  std::cout << std::endl;

  try {
    // Step 1: Launch Media Driver
    std::cout << "1. Launching Media Driver..." << std::endl;
    auto driver = quick::launch_driver();

    // Give driver time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Step 2: Connect Aeron client
    std::cout << "2. Connecting Aeron client..." << std::endl;
    auto aeron_client = quick::connect();

    // Step 3: Create publication and subscription
    std::cout << "3. Creating publication and subscription..." << std::endl;
    const std::string channel = "aeron:udp?endpoint=localhost:40456";
    const std::int32_t stream_id = 1001;

    auto publication = aeron_client->add_publication(channel, stream_id);
    auto subscription = aeron_client->add_subscription(channel, stream_id);

    // Step 4: Wait for connection
    std::cout << "4. Waiting for connection..." << std::endl;
    int connection_attempts = 0;
    while (!publication->is_connected() && connection_attempts < 100) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      connection_attempts++;
    }

    if (!publication->is_connected()) {
      std::cerr << "Failed to connect publication after " << connection_attempts
                << " attempts" << std::endl;
      return 1;
    }

    std::cout << "   Publication connected! Session ID: "
              << publication->session_id() << std::endl;

    // Step 5: Start consumer thread
    std::atomic<bool> consumer_running{true};
    std::atomic<int> messages_received{0};

    std::thread consumer_thread([&]() {
      std::cout << "5. Starting consumer thread..." << std::endl;

      while (consumer_running.load()) {
        int fragments_read = subscription->poll(
            [&](const std::uint8_t *buffer, std::size_t offset,
                std::size_t length, const void *header) {
              std::string message(
                  reinterpret_cast<const char *>(buffer + offset), length);
              std::cout << "   Received: \"" << message
                        << "\" (length: " << length << ")" << std::endl;
              messages_received.fetch_add(1);
            },
            10 // fragment limit
        );

        if (fragments_read == 0) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      }

      std::cout << "   Consumer thread stopped." << std::endl;
    });

    // Give consumer time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Step 6: Send messages
    std::cout << "6. Sending messages..." << std::endl;

    for (int i = 1; i <= 10; ++i) {
      std::string message = "Hello Aeron! Message #" + std::to_string(i);

      client::PublicationResult result = publication->offer(
          reinterpret_cast<const std::uint8_t *>(message.c_str()),
          message.length());

      if (result == client::PublicationResult::SUCCESS) {
        std::cout << "   Sent: \"" << message << "\"" << std::endl;
      } else {
        std::cout << "   Failed to send message " << i
                  << " (result: " << static_cast<std::int64_t>(result) << ")"
                  << std::endl;

        // Retry logic
        int retry_count = 0;
        while (result != client::PublicationResult::SUCCESS &&
               retry_count < 100) {
          std::this_thread::sleep_for(std::chrono::microseconds(100));
          result = publication->offer(
              reinterpret_cast<const std::uint8_t *>(message.c_str()),
              message.length());
          retry_count++;
        }

        if (result == client::PublicationResult::SUCCESS) {
          std::cout << "   Sent after " << retry_count << " retries: \""
                    << message << "\"" << std::endl;
        } else {
          std::cout << "   Failed to send after retries: \"" << message << "\""
                    << std::endl;
        }
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Step 7: Test batch sending
    std::cout << "7. Testing batch sending..." << std::endl;

    std::int64_t claimed_position = publication->try_claim(256);
    if (claimed_position > 0) {
      std::string batch_message = "Batch message with claimed space";
      // Note: In real implementation, we would write directly to the claimed
      // buffer For now, we'll just commit the claim
      publication->commit(claimed_position);
      std::cout << "   Batch message claimed and committed at position: "
                << claimed_position << std::endl;
    } else {
      std::cout << "   Failed to claim space for batch message" << std::endl;
    }

    // Step 8: Wait for all messages to be consumed
    std::cout << "8. Waiting for messages to be consumed..." << std::endl;

    int wait_attempts = 0;
    while (messages_received.load() < 10 && wait_attempts < 100) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      wait_attempts++;
    }

    // Step 9: Display statistics
    std::cout << "9. Final statistics:" << std::endl;
    std::cout << "   Messages sent: 10" << std::endl;
    std::cout << "   Messages received: " << messages_received.load()
              << std::endl;
    std::cout << "   Publication position: " << publication->position()
              << std::endl;
    std::cout << "   Publication position limit: "
              << publication->position_limit() << std::endl;
    std::cout << "   Subscription images: " << subscription->image_count()
              << std::endl;

    // Step 10: Cleanup
    std::cout << "10. Cleaning up..." << std::endl;

    consumer_running.store(false);
    if (consumer_thread.joinable()) {
      consumer_thread.join();
    }

    aeron_client->close_publication(publication);
    aeron_client->close_subscription(subscription);
    aeron_client->close();

    driver->stop();

    std::cout << std::endl;
    std::cout << "=== Example completed successfully! ===" << std::endl;

    return 0;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}
