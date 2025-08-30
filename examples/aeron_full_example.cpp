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
#include <random>
#include <thread>
#include <vector>

using namespace aeron;

/**
 * Advanced Aeron example demonstrating:
 * - Multiple publishers and subscribers
 * - Different message types
 * - Flow control handling
 * - Error recovery
 * - Performance monitoring
 */

struct TestMessage {
  std::uint64_t sequence_number;
  std::uint64_t timestamp;
  std::uint32_t message_type;
  std::uint32_t data_length;
  char data[256];

  TestMessage(std::uint64_t seq, std::uint32_t type, const std::string &msg)
      : sequence_number(seq),
        timestamp(std::chrono::steady_clock::now().time_since_epoch().count()),
        message_type(type),
        data_length(static_cast<std::uint32_t>(msg.length())) {
#ifdef _WIN32
    strncpy_s(data, sizeof(data), msg.c_str(), sizeof(data) - 1);
#else
    std::strncpy(data, msg.c_str(), sizeof(data) - 1);
    data[sizeof(data) - 1] = '\0';
#endif
  }
};

class PerformanceMonitor {
public:
  void record_send(std::size_t bytes) {
    messages_sent_.fetch_add(1);
    bytes_sent_.fetch_add(bytes);
  }

  void record_receive(std::size_t bytes) {
    messages_received_.fetch_add(1);
    bytes_received_.fetch_add(bytes);
  }

  void print_stats() const {
    std::cout << "Performance Statistics:" << std::endl;
    std::cout << "  Messages sent: " << messages_sent_.load() << std::endl;
    std::cout << "  Messages received: " << messages_received_.load()
              << std::endl;
    std::cout << "  Bytes sent: " << bytes_sent_.load() << std::endl;
    std::cout << "  Bytes received: " << bytes_received_.load() << std::endl;
    std::cout << "  Message loss: "
              << (messages_sent_.load() - messages_received_.load())
              << std::endl;
  }

private:
  std::atomic<std::uint64_t> messages_sent_{0};
  std::atomic<std::uint64_t> messages_received_{0};
  std::atomic<std::uint64_t> bytes_sent_{0};
  std::atomic<std::uint64_t> bytes_received_{0};
};

int main() {
  std::cout << "=== Aeron C++ Full Example ===" << std::endl;
  std::cout << "Version: " << Version::get_full_version_string() << std::endl;
  std::cout << std::endl;

  try {
    // Configuration
    const std::string channel1 = "aeron:udp?endpoint=localhost:40456";
    const std::string channel2 = "aeron:udp?endpoint=localhost:40457";
    const std::int32_t stream_id1 = 1001;
    const std::int32_t stream_id2 = 1002;
    const int num_publishers = 2;
    const int num_subscribers = 2;
    const int messages_per_publisher = 100;

    PerformanceMonitor monitor;

    // Step 1: Launch Media Driver with custom configuration
    std::cout << "1. Launching Media Driver with custom configuration..."
              << std::endl;

    driver::MediaDriverContext context;
    context.mtu_length = 1408;
    context.term_buffer_length = 128 * 1024; // 128KB
    context.socket_rcvbuf = 256 * 1024;      // 256KB
    context.socket_sndbuf = 256 * 1024;      // 256KB

    auto driver = driver::MediaDriver::launch(context);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Step 2: Connect multiple Aeron clients
    std::cout << "2. Connecting Aeron clients..." << std::endl;

    std::vector<std::shared_ptr<client::Aeron>> clients;
    for (int i = 0; i < num_publishers + num_subscribers; ++i) {
      clients.push_back(client::Aeron::connect());
    }

    // Step 3: Create publications
    std::cout << "3. Creating publications..." << std::endl;

    std::vector<std::shared_ptr<client::Publication>> publications;
    publications.push_back(clients[0]->add_publication(channel1, stream_id1));
    publications.push_back(clients[1]->add_publication(channel2, stream_id2));

    // Step 4: Create subscriptions
    std::cout << "4. Creating subscriptions..." << std::endl;

    std::vector<std::shared_ptr<client::Subscription>> subscriptions;
    subscriptions.push_back(clients[2]->add_subscription(channel1, stream_id1));
    subscriptions.push_back(clients[3]->add_subscription(channel2, stream_id2));

    // Step 5: Wait for connections
    std::cout << "5. Waiting for connections..." << std::endl;

    for (auto &pub : publications) {
      int attempts = 0;
      while (!pub->is_connected() && attempts < 200) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        attempts++;
      }
      if (pub->is_connected()) {
        std::cout << "   Publication connected on channel: " << pub->channel()
                  << ", session: " << pub->session_id() << std::endl;
      } else {
        std::cout << "   Warning: Publication failed to connect: "
                  << pub->channel() << std::endl;
      }
    }

    // Step 6: Start subscriber threads
    std::cout << "6. Starting subscriber threads..." << std::endl;

    std::atomic<bool> subscribers_running{true};
    std::vector<std::thread> subscriber_threads;

    for (size_t i = 0; i < subscriptions.size(); ++i) {
      subscriber_threads.emplace_back([&, i]() {
        auto &subscription = subscriptions[i];
        std::cout << "   Subscriber " << i << " started for stream "
                  << subscription->stream_id() << std::endl;

        while (subscribers_running.load()) {
          int fragments_read = subscription->poll(
              [&](const std::uint8_t *buffer, std::size_t offset,
                  std::size_t length, const void * /*header*/) {
                if (length >= sizeof(TestMessage)) {
                  const TestMessage *msg =
                      reinterpret_cast<const TestMessage *>(buffer + offset);
                  std::cout << "   Sub" << i
                            << " received: seq=" << msg->sequence_number
                            << ", type=" << msg->message_type << ", data=\""
                            << msg->data << "\"" << std::endl;
                  monitor.record_receive(length);
                }
              },
              20 // fragment limit
          );

          if (fragments_read == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
          }
        }

        std::cout << "   Subscriber " << i << " stopped." << std::endl;
      });
    }

    // Give subscribers time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Step 7: Start publisher threads
    std::cout << "7. Starting publisher threads..." << std::endl;

    std::vector<std::thread> publisher_threads;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> delay_dist(1, 10);

    for (size_t i = 0; i < publications.size(); ++i) {
      publisher_threads.emplace_back([&, i]() {
        auto &publication = publications[i];
        std::cout << "   Publisher " << i << " started for stream "
                  << publication->stream_id() << std::endl;

        for (int msg_num = 1; msg_num <= messages_per_publisher; ++msg_num) {
          TestMessage message(msg_num, static_cast<std::uint32_t>(i + 1),
                              "Message from publisher " + std::to_string(i) +
                                  " #" + std::to_string(msg_num));

          client::PublicationResult result = publication->offer(
              reinterpret_cast<const std::uint8_t *>(&message),
              sizeof(message));

          if (result == client::PublicationResult::SUCCESS) {
            monitor.record_send(sizeof(message));
          } else {
            // Retry with backoff
            int retry_count = 0;
            while (result != client::PublicationResult::SUCCESS &&
                   retry_count < 50) {
              std::this_thread::sleep_for(
                  std::chrono::microseconds(100 * (retry_count + 1)));
              result = publication->offer(
                  reinterpret_cast<const std::uint8_t *>(&message),
                  sizeof(message));
              retry_count++;
            }

            if (result == client::PublicationResult::SUCCESS) {
              monitor.record_send(sizeof(message));
              std::cout << "   Pub" << i << " sent after " << retry_count
                        << " retries: #" << msg_num << std::endl;
            } else {
              std::cout << "   Pub" << i << " failed to send: #" << msg_num
                        << " (result: " << static_cast<std::int64_t>(result)
                        << ")" << std::endl;
            }
          }

          // Random delay between messages
          std::this_thread::sleep_for(
              std::chrono::milliseconds(delay_dist(gen)));
        }

        std::cout << "   Publisher " << i << " completed." << std::endl;
      });
    }

    // Step 8: Wait for publishers to complete
    std::cout << "8. Waiting for publishers to complete..." << std::endl;

    for (auto &thread : publisher_threads) {
      if (thread.joinable()) {
        thread.join();
      }
    }

    // Step 9: Wait for remaining messages to be consumed
    std::cout << "9. Waiting for remaining messages to be consumed..."
              << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Step 10: Stop subscribers
    std::cout << "10. Stopping subscribers..." << std::endl;

    subscribers_running.store(false);
    for (auto &thread : subscriber_threads) {
      if (thread.joinable()) {
        thread.join();
      }
    }

    // Step 11: Display final statistics
    std::cout << "11. Final statistics:" << std::endl;
    monitor.print_stats();

    std::cout << "\nPublication Statistics:" << std::endl;
    for (size_t i = 0; i < publications.size(); ++i) {
      auto &pub = publications[i];
      std::cout << "  Publication " << i << ":" << std::endl;
      std::cout << "    Position: " << pub->position() << std::endl;
      std::cout << "    Position Limit: " << pub->position_limit() << std::endl;
      std::cout << "    Max Possible Position: " << pub->max_possible_position()
                << std::endl;
      std::cout << "    Connected: " << (pub->is_connected() ? "Yes" : "No")
                << std::endl;
    }

    std::cout << "\nSubscription Statistics:" << std::endl;
    for (size_t i = 0; i < subscriptions.size(); ++i) {
      auto &sub = subscriptions[i];
      std::cout << "  Subscription " << i << ":" << std::endl;
      std::cout << "    Images: " << sub->image_count() << std::endl;
      std::cout << "    Connected: " << (sub->is_connected() ? "Yes" : "No")
                << std::endl;
    }

    // Step 12: Cleanup
    std::cout << "12. Cleaning up..." << std::endl;

    for (size_t i = 0; i < publications.size(); ++i) {
      clients[i]->close_publication(publications[i]);
    }

    for (size_t i = 0; i < subscriptions.size(); ++i) {
      clients[i + num_publishers]->close_subscription(subscriptions[i]);
    }

    for (auto &client : clients) {
      client->close();
    }

    driver->stop();

    std::cout << std::endl;
    std::cout << "=== Full example completed successfully! ===" << std::endl;

    return 0;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
}
