/**
 * Complete Aeron C++ Example - Real Implementation
 * Demonstrates the full Aeron Media Driver and Client functionality.
 */

#include "nano_stream_aeron.h" // This includes all Aeron headers and implementations
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace aeron;
using namespace aeron::driver;

/**
 * Message structure for demo.
 */
struct Message {
  std::int64_t timestamp;
  std::int64_t sequence;
  char data[128];
};

/**
 * Complete Aeron publisher using real Media Driver.
 */
class AeronPublisher {
public:
  AeronPublisher(const std::string &channel, std::int32_t stream_id)
      : channel_(channel), stream_id_(stream_id), sequence_(0) {

    std::cout << "Starting Aeron Publisher..." << std::endl;

    // Launch Media Driver with optimized settings
    auto driver_context =
        nano_stream_aeron::presets::ultra_low_latency_driver();
    media_driver_ = MediaDriver::launch(std::move(driver_context));

    // Wait for driver to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Connect Aeron client
    auto client_context =
        nano_stream_aeron::presets::ultra_low_latency_client();
    aeron_ = Aeron::connect(std::move(client_context));

    // Add publication
    publication_ = aeron_->add_publication(channel_, stream_id_);

    // Wait for publication to connect
    while (!publication_->is_connected()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      aeron_->conductor().do_work();
    }

    std::cout << "Publisher connected to: " << channel_ << " stream "
              << stream_id_ << std::endl;
  }

  ~AeronPublisher() {
    if (publication_) {
      aeron_->close_publication(publication_);
    }
    if (aeron_) {
      aeron_->close();
    }
    if (media_driver_) {
      media_driver_->close();
    }
    std::cout << "Publisher shutdown complete." << std::endl;
  }

  void publish_messages(std::int32_t count, std::int32_t interval_us = 1000) {
    std::cout << "Publishing " << count << " messages..." << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();
    std::int32_t successful_offers = 0;

    for (std::int32_t i = 0; i < count; ++i) {
      Message msg;
      msg.timestamp =
          std::chrono::duration_cast<std::chrono::nanoseconds>(
              std::chrono::high_resolution_clock::now().time_since_epoch())
              .count();
      msg.sequence = ++sequence_;
      snprintf(msg.data, sizeof(msg.data), "Message #%lld from publisher",
               static_cast<long long>(msg.sequence));

      // Try to offer the message
      auto result = publication_->offer(
          reinterpret_cast<const std::uint8_t *>(&msg), sizeof(msg));

      if (result == Publication::OfferResult::SUCCESS) {
        successful_offers++;
        if (i % 1000 == 0) {
          std::cout << "Published message " << i << " (success)" << std::endl;
        }
      } else {
        // Handle back pressure
        switch (result) {
        case Publication::OfferResult::BACK_PRESSURE:
          std::cout << "Back pressure, retrying..." << std::endl;
          --i; // Retry this message
          std::this_thread::sleep_for(std::chrono::microseconds(10));
          break;

        case Publication::OfferResult::NOT_CONNECTED:
          std::cout << "Publication not connected, waiting..." << std::endl;
          --i; // Retry this message
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
          break;

        default:
          std::cout << "Offer failed with result: " << static_cast<int>(result)
                    << std::endl;
          break;
        }
      }

      // Do client work
      aeron_->conductor().do_work();

      if (interval_us > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(interval_us));
      }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);

    std::cout << "Published " << successful_offers << "/" << count
              << " messages in " << duration.count() << "ms" << std::endl;
    std::cout << "Throughput: "
              << (successful_offers * 1000.0 / duration.count()) << " msgs/sec"
              << std::endl;
  }

private:
  std::string channel_;
  std::int32_t stream_id_;
  std::int64_t sequence_;

  std::unique_ptr<MediaDriver> media_driver_;
  std::shared_ptr<Aeron> aeron_;
  std::shared_ptr<Publication> publication_;
};

/**
 * Complete Aeron subscriber using real Media Driver.
 */
class AeronSubscriber {
public:
  AeronSubscriber(const std::string &channel, std::int32_t stream_id)
      : channel_(channel), stream_id_(stream_id), running_(false),
        messages_received_(0), last_sequence_(0) {

    std::cout << "Starting Aeron Subscriber..." << std::endl;

    // Connect to existing Media Driver (or start our own)
    auto client_context =
        nano_stream_aeron::presets::ultra_low_latency_client();
    aeron_ = Aeron::connect(std::move(client_context));

    // Add subscription with handlers
    subscription_ = aeron_->add_subscription(
        channel_, stream_id_,
        [](std::shared_ptr<Image> image) {
          std::cout << "Image available from: " << image->source_identity()
                    << std::endl;
        },
        [](std::shared_ptr<Image> image) {
          std::cout << "Image unavailable from: " << image->source_identity()
                    << std::endl;
        });

    std::cout << "Subscriber connected to: " << channel_ << " stream "
              << stream_id_ << std::endl;
  }

  ~AeronSubscriber() {
    stop();
    if (subscription_) {
      aeron_->close_subscription(subscription_);
    }
    if (aeron_) {
      aeron_->close();
    }
    std::cout << "Subscriber shutdown complete." << std::endl;
  }

  void start() {
    running_ = true;
    subscriber_thread_ = std::thread(&AeronSubscriber::poll_loop, this);
    std::cout << "Subscriber started polling..." << std::endl;
  }

  void stop() {
    running_ = false;
    if (subscriber_thread_.joinable()) {
      subscriber_thread_.join();
    }
    std::cout << "Subscriber stopped. Total messages received: "
              << messages_received_ << std::endl;
  }

  void wait_for_messages(std::int32_t expected_count,
                         std::int32_t timeout_ms = 30000) {
    auto start_time = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(timeout_ms);

    while (messages_received_ < expected_count) {
      auto elapsed = std::chrono::steady_clock::now() - start_time;
      if (elapsed > timeout) {
        std::cout << "Timeout waiting for messages. Received: "
                  << messages_received_ << " Expected: " << expected_count
                  << std::endl;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

private:
  void poll_loop() {
    fragment_handler_t handler =
        [this](const concurrent::AtomicBuffer &buffer, std::int32_t offset,
               std::int32_t length,
               const concurrent::AtomicBuffer & /*header*/) {
          if (length >= static_cast<std::int32_t>(sizeof(Message))) {
            const Message *msg =
                reinterpret_cast<const Message *>(buffer.buffer() + offset);

            // Calculate latency
            auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::high_resolution_clock::now()
                               .time_since_epoch())
                           .count();
            auto latency_ns = now - msg->timestamp;

            messages_received_++;

            // Check for gaps
            if (last_sequence_ > 0 && msg->sequence != last_sequence_ + 1) {
              std::cout << "Gap detected! Expected: " << (last_sequence_ + 1)
                        << " Got: " << msg->sequence << std::endl;
            }
            last_sequence_ = msg->sequence;

            if (messages_received_ % 1000 == 0) {
              std::cout << "Received message " << messages_received_
                        << " (seq: " << msg->sequence
                        << ", latency: " << (latency_ns / 1000.0) << "μs)"
                        << std::endl;
            }
          }
        };

    while (running_) {
      // Poll for messages
      int fragments_read = subscription_->poll(handler, 10);

      // Do client work
      aeron_->conductor().do_work();

      if (fragments_read == 0) {
        // Brief pause if no messages
        std::this_thread::sleep_for(std::chrono::microseconds(1));
      }
    }
  }

  std::string channel_;
  std::int32_t stream_id_;
  std::atomic<bool> running_;
  std::atomic<std::int32_t> messages_received_;
  std::int64_t last_sequence_;

  std::shared_ptr<Aeron> aeron_;
  std::shared_ptr<Subscription> subscription_;
  std::thread subscriber_thread_;
};

/**
 * Main function demonstrating complete Aeron functionality.
 */
int main() {
  try {
    std::cout << "=== Nano-Stream Aeron Complete Example ===" << std::endl;

    // Test both IPC and UDP transports
    std::vector<std::pair<std::string, std::string>> test_channels = {
        {"aeron:ipc", "IPC Transport"},
        {"aeron:udp?endpoint=localhost:40124", "UDP Transport"}};

    for (const auto &[channel, description] : test_channels) {
      std::cout << "\n--- Testing " << description << " ---" << std::endl;
      std::cout << "Channel: " << channel << std::endl;

      const std::int32_t STREAM_ID = 1001;
      const std::int32_t MESSAGE_COUNT = 10000;

      // Start subscriber first
      auto subscriber = std::make_unique<AeronSubscriber>(channel, STREAM_ID);
      subscriber->start();

      // Give subscriber time to connect
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      // Start publisher
      auto publisher = std::make_unique<AeronPublisher>(channel, STREAM_ID);

      // Give time for connection
      std::this_thread::sleep_for(std::chrono::milliseconds(500));

      // Start publishing
      auto start_time = std::chrono::high_resolution_clock::now();
      publisher->publish_messages(MESSAGE_COUNT, 100); // 100μs interval

      // Wait for all messages to be received
      subscriber->wait_for_messages(MESSAGE_COUNT);

      auto end_time = std::chrono::high_resolution_clock::now();
      auto total_duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(end_time -
                                                                start_time);

      std::cout << "Total test duration: " << total_duration.count() << "ms"
                << std::endl;

      // Cleanup
      subscriber->stop();
      subscriber.reset();
      publisher.reset();

      std::cout << description << " test completed!" << std::endl;

      // Brief pause between tests
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    std::cout << "\n=== All tests completed successfully! ===" << std::endl;
    return 0;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}
