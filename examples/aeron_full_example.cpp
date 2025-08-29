/**
 * Complete Aeron C++ Example
 * Demonstrates the full Aeron API with publications and subscriptions.
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

using namespace aeron;

/**
 * Simple message structure for demo.
 */
struct DemoMessage {
  std::int64_t timestamp;
  std::int32_t sequence;
  char data[256];
};

/**
 * Publisher example using Aeron.
 */
class AeronPublisher {
public:
  AeronPublisher(const std::string &channel, std::int32_t stream_id)
      : channel_(channel), stream_id_(stream_id) {

    // Create context with custom settings
    auto context = std::make_unique<Context>();
    context->aeron_dir("/tmp/aeron")
        .publication_term_buffer_length(64 * 1024)
        .error_handler([](const std::exception &e) {
          std::cerr << "Aeron error: " << e.what() << std::endl;
        });

    // Connect to Aeron
    aeron_ = Aeron::connect(std::move(context));

    // Add publication
    publication_ = aeron_->add_publication(channel_, stream_id_);

    std::cout << "Publisher created for channel: " << channel_
              << ", stream: " << stream_id_ << std::endl;
  }

  void publish_messages(int count) {
    std::cout << "Publishing " << count << " messages..." << std::endl;

    for (int i = 0; i < count; ++i) {
      DemoMessage msg;
      msg.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          std::chrono::steady_clock::now().time_since_epoch())
                          .count();
      msg.sequence = i;
      snprintf(msg.data, sizeof(msg.data), "Hello Aeron! Message #%d", i);

      // Offer the message
      auto result = publication_->offer(
          reinterpret_cast<const std::uint8_t *>(&msg), sizeof(msg));

      if (result == Publication::OfferResult::SUCCESS) {
        std::cout << "Sent message " << i << std::endl;
      } else if (result == Publication::OfferResult::BACK_PRESSURE) {
        std::cout << "Back pressure on message " << i << ", retrying..."
                  << std::endl;
        --i; // Retry
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      } else {
        std::cout << "Failed to send message " << i
                  << " (code: " << static_cast<std::int64_t>(result) << ")"
                  << std::endl;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }

  void close() {
    if (publication_) {
      aeron_->close_publication(publication_);
      publication_.reset();
    }
    if (aeron_) {
      aeron_->close();
      aeron_.reset();
    }
  }

private:
  std::string channel_;
  std::int32_t stream_id_;
  std::shared_ptr<Aeron> aeron_;
  std::shared_ptr<Publication> publication_;
};

/**
 * Subscriber example using Aeron.
 */
class AeronSubscriber {
public:
  AeronSubscriber(const std::string &channel, std::int32_t stream_id)
      : channel_(channel), stream_id_(stream_id), messages_received_(0) {

    // Create context with handlers
    auto context = std::make_unique<Context>();
    context->aeron_dir("/tmp/aeron")
        .available_image_handler([](std::shared_ptr<Image> image) {
          std::cout << "Image available: " << image->source_identity()
                    << std::endl;
        })
        .unavailable_image_handler([](std::shared_ptr<Image> image) {
          std::cout << "Image unavailable: " << image->source_identity()
                    << std::endl;
        })
        .error_handler([](const std::exception &e) {
          std::cerr << "Aeron error: " << e.what() << std::endl;
        });

    // Connect to Aeron
    aeron_ = Aeron::connect(std::move(context));

    // Add subscription
    subscription_ = aeron_->add_subscription(channel_, stream_id_);

    std::cout << "Subscriber created for channel: " << channel_
              << ", stream: " << stream_id_ << std::endl;
  }

  void poll_messages(int max_messages = 10) {
    auto handler = [this](const concurrent::AtomicBuffer &buffer,
                          std::int32_t offset, std::int32_t length,
                          const concurrent::AtomicBuffer & /*header*/) {
      if (length >= static_cast<std::int32_t>(sizeof(DemoMessage))) {
        DemoMessage msg;
        buffer.get_bytes(offset, reinterpret_cast<std::uint8_t *>(&msg),
                         sizeof(msg));

        auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                       .count();
        auto latency = now - msg.timestamp;

        std::cout << "Received message " << msg.sequence << ": " << msg.data
                  << " (latency: " << latency << " ns)" << std::endl;

        ++messages_received_;
      }
    };

    int fragments = subscription_->poll(handler, max_messages);
    if (fragments > 0) {
      std::cout << "Processed " << fragments << " fragments" << std::endl;
    }
  }

  void run_polling_loop(std::chrono::seconds duration) {
    std::cout << "Starting polling loop for " << duration.count()
              << " seconds..." << std::endl;

    auto start_time = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start_time < duration) {
      poll_messages();
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "Polling loop finished. Total messages received: "
              << messages_received_ << std::endl;
  }

  void close() {
    if (subscription_) {
      aeron_->close_subscription(subscription_);
      subscription_.reset();
    }
    if (aeron_) {
      aeron_->close();
      aeron_.reset();
    }
  }

private:
  std::string channel_;
  std::int32_t stream_id_;
  std::shared_ptr<Aeron> aeron_;
  std::shared_ptr<Subscription> subscription_;
  std::atomic<int> messages_received_;
};

/**
 * Example demonstrating zero-copy publishing with BufferClaim.
 */
void zero_copy_example() {
  std::cout << "\n=== Zero-Copy Publishing Example ===" << std::endl;

  // Setup Aeron
  auto aeron = Aeron::connect();
  auto publication = aeron->add_publication("aeron:ipc", 1002);

  // Use BufferClaim for zero-copy writing
  BufferClaim buffer_claim;

  for (int i = 0; i < 5; ++i) {
    std::int64_t position =
        publication->try_claim(sizeof(DemoMessage), buffer_claim.buffer());

    if (position > 0) {
      // Write directly into the claimed buffer
      DemoMessage *msg = reinterpret_cast<DemoMessage *>(
          buffer_claim.buffer().buffer() + buffer_claim.offset());

      msg->timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();
      msg->sequence = i;
      snprintf(msg->data, sizeof(msg->data), "Zero-copy message #%d", i);

      // Commit the claim
      buffer_claim.commit();

      std::cout << "Zero-copy sent message " << i << std::endl;
    } else {
      std::cout << "Failed to claim buffer for message " << i << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  aeron->close();
}

/**
 * Main function demonstrating various Aeron features.
 */
int main() {
  try {
    std::cout << "=== Aeron C++ Full Example ===" << std::endl;

    const std::string channel = "aeron:ipc";
    const std::int32_t stream_id = 1001;

    // Start subscriber in separate thread
    std::thread subscriber_thread([&]() {
      AeronSubscriber subscriber(channel, stream_id);
      subscriber.run_polling_loop(std::chrono::seconds(10));
      subscriber.close();
    });

    // Give subscriber time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Run publisher
    AeronPublisher publisher(channel, stream_id);
    publisher.publish_messages(10);
    publisher.close();

    // Wait for subscriber to finish
    subscriber_thread.join();

    // Demonstrate zero-copy
    zero_copy_example();

    std::cout << "\n=== Example completed successfully ===" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
