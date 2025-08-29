#pragma once

/**
 * Simplified Aeron IPC implementation using nano_stream as the foundation.
 * This version focuses on getting basic IPC functionality working quickly.
 */

#include "../nano_stream/nano_stream.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <thread>

namespace aeron {
namespace simple {

/**
 * Simple event structure for IPC communication.
 * Must be trivially copyable for shared memory usage.
 */
struct SimpleEvent {
  int64_t id;
  int64_t timestamp;
  double value;
  char message[64];

  SimpleEvent() : id(0), timestamp(0), value(0.0) { message[0] = '\0'; }

  SimpleEvent(int64_t id_val, double value_val, const std::string &msg = "")
      : id(id_val), value(value_val) {
    timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch())
                    .count();
#ifdef _WIN32
    strncpy_s(message, sizeof(message), msg.c_str(), sizeof(message) - 1);
#else
    std::strncpy(message, msg.c_str(), sizeof(message) - 1);
    message[sizeof(message) - 1] = '\0';
#endif
  }
};

static_assert(std::is_trivially_copyable_v<SimpleEvent>,
              "SimpleEvent must be trivially copyable");

/**
 * Simple IPC publication using nano_stream RingBuffer.
 * For now, this uses in-process communication with the plan to extend to IPC
 * later.
 */
template <typename T = SimpleEvent> class SimplePublication {
public:
  static_assert(std::is_trivially_copyable_v<T>,
                "Type must be trivially copyable for IPC");

  /**
   * Create a publication with the specified buffer size.
   */
  explicit SimplePublication(size_t buffer_size = 1024)
      : ring_buffer_(nano_stream::RingBuffer<T>::createSingleProducer(
            buffer_size, []() { return T{}; })) {}

  /**
   * Offer an event for publication.
   *
   * @param event The event to publish
   * @return true if published successfully, false if insufficient capacity
   */
  bool offer(const T &event) {
    try {
      int64_t sequence = ring_buffer_.try_next();
      T &slot = ring_buffer_.get(sequence);
      slot = event;
      ring_buffer_.publish(sequence);
      return true;
    } catch (const nano_stream::InsufficientCapacityException &) {
      return false;
    }
  }

  /**
   * Try to claim space for n events.
   */
  std::optional<int64_t> try_claim(int n = 1) {
    try {
      return ring_buffer_.try_next(n);
    } catch (const nano_stream::InsufficientCapacityException &) {
      return std::nullopt;
    }
  }

  /**
   * Get event at the specified sequence.
   */
  T &get(int64_t sequence) { return ring_buffer_.get(sequence); }

  /**
   * Publish the claimed sequence.
   */
  void publish(int64_t sequence) { ring_buffer_.publish(sequence); }

  /**
   * Get remaining capacity.
   */
  size_t remaining_capacity() { return ring_buffer_.remaining_capacity(); }

  /**
   * Get buffer size.
   */
  size_t get_buffer_size() const { return ring_buffer_.get_buffer_size(); }

  /**
   * Get the underlying ring buffer for subscription setup.
   */
  nano_stream::RingBuffer<T> &get_ring_buffer() { return ring_buffer_; }

private:
  nano_stream::RingBuffer<T> ring_buffer_;
};

/**
 * Simple IPC subscription using nano_stream Consumer.
 */
template <typename T = SimpleEvent> class SimpleSubscription {
public:
  using fragment_handler_t =
      std::function<void(const T &, int64_t sequence, bool end_of_batch)>;
  using event_handler_t = std::function<void(const T &)>;

  /**
   * Create a subscription connected to the given publication.
   */
  explicit SimpleSubscription(SimplePublication<T> &publication)
      : publication_(publication),
        last_read_sequence_(nano_stream::Sequence::INITIAL_VALUE) {}

  /**
   * Poll for new events and process them with the provided handler.
   *
   * @param handler Function to process each event
   * @param max_count Maximum number of events to process in this call
   * @return Number of events processed
   */
  int poll(fragment_handler_t handler, int max_count = 10) {
    int processed = 0;
    int64_t next_sequence = last_read_sequence_ + 1;

    while (processed < max_count) {
      if (!publication_.get_ring_buffer().is_available(next_sequence)) {
        break; // No more events available
      }

      const T &event = publication_.get_ring_buffer().get(next_sequence);
      bool end_of_batch =
          (processed == max_count - 1) ||
          !publication_.get_ring_buffer().is_available(next_sequence + 1);

      handler(event, next_sequence, end_of_batch);

      last_read_sequence_ = next_sequence;
      ++next_sequence;
      ++processed;
    }

    return processed;
  }

  /**
   * Try to read a single event without blocking.
   */
  std::optional<T> try_read() {
    int64_t next_sequence = last_read_sequence_ + 1;
    if (!publication_.get_ring_buffer().is_available(next_sequence)) {
      return std::nullopt;
    }

    const T &event = publication_.get_ring_buffer().get(next_sequence);
    last_read_sequence_ = next_sequence;
    return event;
  }

  /**
   * Check if there are events available to read.
   */
  bool has_events() const {
    return publication_.get_ring_buffer().is_available(last_read_sequence_ + 1);
  }

  /**
   * Get the number of events available to read.
   */
  int64_t available_events() const {
    int64_t cursor = publication_.get_ring_buffer().get_cursor();
    return cursor - last_read_sequence_;
  }

  /**
   * Get the current read position.
   */
  int64_t get_position() const { return last_read_sequence_; }

  /**
   * Reset the read position to the beginning.
   */
  void reset_position() {
    last_read_sequence_ = nano_stream::Sequence::INITIAL_VALUE;
  }

private:
  SimplePublication<T> &publication_;
  int64_t last_read_sequence_;
};

/**
 * Convenience class for automatic polling in a background thread.
 */
template <typename T = SimpleEvent> class PollingSubscription {
public:
  using event_handler_t = std::function<void(const T &)>;

  /**
   * Create a polling subscription.
   */
  PollingSubscription(SimplePublication<T> &publication,
                      event_handler_t handler)
      : subscription_(publication), handler_(std::move(handler)),
        running_(false) {}

  /**
   * Start polling for events in a background thread.
   */
  void start_polling(
      std::chrono::milliseconds poll_interval = std::chrono::milliseconds(1)) {
    if (running_.exchange(true)) {
      return; // Already running
    }

    polling_thread_ = std::thread([this, poll_interval]() {
      while (running_.load()) {
        int processed = subscription_.poll(
            [this](const T &event, int64_t, bool) { handler_(event); }, 100);

        if (processed == 0) {
          std::this_thread::sleep_for(poll_interval);
        }
      }
    });
  }

  /**
   * Stop polling.
   */
  void stop_polling() {
    running_.store(false);
    if (polling_thread_.joinable()) {
      polling_thread_.join();
    }
  }

  /**
   * Check if currently polling.
   */
  bool is_polling() const { return running_.load(); }

  /**
   * Get the underlying subscription.
   */
  SimpleSubscription<T> &get_subscription() { return subscription_; }

  ~PollingSubscription() { stop_polling(); }

private:
  SimpleSubscription<T> subscription_;
  event_handler_t handler_;
  std::atomic<bool> running_;
  std::thread polling_thread_;
};

/**
 * Simple Aeron client interface.
 */
class SimpleAeron {
public:
  /**
   * Create a simple publication.
   */
  template <typename T = SimpleEvent>
  static std::unique_ptr<SimplePublication<T>>
  create_publication(size_t buffer_size = 1024) {
    return std::make_unique<SimplePublication<T>>(buffer_size);
  }

  /**
   * Create a simple subscription.
   */
  template <typename T = SimpleEvent>
  static std::unique_ptr<SimpleSubscription<T>>
  create_subscription(SimplePublication<T> &publication) {
    return std::make_unique<SimpleSubscription<T>>(publication);
  }

  /**
   * Create a polling subscription.
   */
  template <typename T = SimpleEvent>
  static std::unique_ptr<PollingSubscription<T>> create_polling_subscription(
      SimplePublication<T> &publication,
      typename PollingSubscription<T>::event_handler_t handler) {
    return std::make_unique<PollingSubscription<T>>(publication,
                                                    std::move(handler));
  }
};

/**
 * Version information.
 */
struct Version {
  static constexpr int MAJOR = 1;
  static constexpr int MINOR = 0;
  static constexpr int PATCH = 0;

  static std::string get_version_string() {
    return std::to_string(MAJOR) + "." + std::to_string(MINOR) + "." +
           std::to_string(PATCH);
  }
};

} // namespace simple
} // namespace aeron
