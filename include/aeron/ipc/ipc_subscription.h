#pragma once

#include "../../nano_stream/consumer.h"
#include "../../nano_stream/ring_buffer.h"
#include "../util/memory_mapped_file.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace aeron {
namespace ipc {

/**
 * IPC Subscription for inter-process communication using shared memory.
 * Receives events published by IpcPublication.
 */
template <typename T> class IpcSubscription {
public:
  using fragment_handler_t =
      std::function<void(const T &, int64_t sequence, bool end_of_batch)>;

  /**
   * Connect to an existing IPC publication.
   *
   * @param channel_name Name of the IPC channel to subscribe to
   */
  explicit IpcSubscription(const std::string &channel_name)
      : channel_name_(channel_name),
        memory_map_(get_shared_memory_path(channel_name), 0, false),
        last_read_sequence_(nano_stream::Sequence::INITIAL_VALUE) {
    if (!memory_map_.is_valid()) {
      throw std::runtime_error(
          "Failed to connect to shared memory for channel: " + channel_name);
    }

    connect_to_publication();
  }

  /**
   * Poll for new events and process them with the provided handler.
   *
   * @param handler Function to process each event
   * @param max_count Maximum number of events to process in this call
   * @return Number of events processed
   */
  int poll(fragment_handler_t handler, int max_count = 10) {
    if (!is_connected()) {
      return 0;
    }

    int processed = 0;
    int64_t next_sequence = last_read_sequence_ + 1;

    while (processed < max_count) {
      if (!ring_buffer_->is_available(next_sequence)) {
        break; // No more events available
      }

      const T &event = ring_buffer_->get(next_sequence);
      bool end_of_batch = (processed == max_count - 1) ||
                          !ring_buffer_->is_available(next_sequence + 1);

      handler(event, next_sequence, end_of_batch);

      last_read_sequence_ = next_sequence;
      ++next_sequence;
      ++processed;
    }

    return processed;
  }

  /**
   * Try to read a single event without blocking.
   *
   * @return Optional containing the event if available
   */
  std::optional<T> try_read() {
    if (!is_connected()) {
      return std::nullopt;
    }

    int64_t next_sequence = last_read_sequence_ + 1;
    if (!ring_buffer_->is_available(next_sequence)) {
      return std::nullopt;
    }

    const T &event = ring_buffer_->get(next_sequence);
    last_read_sequence_ = next_sequence;

    if constexpr (std::is_trivially_copyable_v<T>) {
      return event;
    } else {
      // For non-trivially copyable types, we need copy constructor
      return T(event);
    }
  }

  /**
   * Check if there are events available to read.
   *
   * @return true if events are available, false otherwise
   */
  bool has_events() const {
    if (!is_connected()) {
      return false;
    }

    return ring_buffer_->is_available(last_read_sequence_ + 1);
  }

  /**
   * Get the number of events available to read.
   *
   * @return Number of available events
   */
  int64_t available_events() const {
    if (!is_connected()) {
      return 0;
    }

    int64_t cursor = ring_buffer_->get_cursor();
    return cursor - last_read_sequence_;
  }

  /**
   * Check if the subscription is connected to a publication.
   */
  bool is_connected() const {
    return memory_map_.is_valid() && ring_buffer_ != nullptr;
  }

  /**
   * Get the channel name.
   */
  const std::string &get_channel_name() const { return channel_name_; }

  /**
   * Get buffer size.
   */
  size_t get_buffer_size() const { return buffer_size_; }

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

  /**
   * Seek to a specific position.
   *
   * @param sequence The sequence number to seek to
   */
  void seek_to(int64_t sequence) { last_read_sequence_ = sequence; }

private:
  std::string channel_name_;
  util::MemoryMappedFile memory_map_;
  std::unique_ptr<nano_stream::RingBuffer<T>> ring_buffer_;
  size_t buffer_size_;
  int64_t last_read_sequence_;

  // Shared memory layout constants (same as IpcPublication)
  static constexpr size_t HEADER_SIZE = 64;
  static constexpr size_t MAGIC_OFFSET = 0;
  static constexpr size_t VERSION_OFFSET = 8;
  static constexpr size_t BUFFER_SIZE_OFFSET = 16;
  static constexpr size_t TYPE_SIZE_OFFSET = 24;
  static constexpr size_t RING_BUFFER_OFFSET = HEADER_SIZE;

  static constexpr uint64_t MAGIC_NUMBER = 0x41455244'4F4E414EULL; // "NANO-AER"
  static constexpr uint32_t VERSION = 1;

  /**
   * Generate shared memory file path for the channel.
   */
  static std::string get_shared_memory_path(const std::string &channel_name) {
    if constexpr (util::MemoryMappedFile::is_windows()) {
      return "Global\\nano_stream_" + channel_name;
    } else {
      return "/tmp/nano_stream_" + channel_name + ".shm";
    }
  }

  /**
   * Connect to existing publication's shared memory.
   */
  void connect_to_publication() {
    void *base_addr = memory_map_.get_address();

    // Verify header
    util::SharedAtomic<uint64_t> magic(
        static_cast<uint64_t *>(static_cast<char *>(base_addr) + MAGIC_OFFSET));
    util::SharedAtomic<uint32_t> version(static_cast<uint32_t *>(
        static_cast<char *>(base_addr) + VERSION_OFFSET));
    util::SharedAtomic<uint64_t> buffer_size(static_cast<uint64_t *>(
        static_cast<char *>(base_addr) + BUFFER_SIZE_OFFSET));
    util::SharedAtomic<uint64_t> type_size(static_cast<uint64_t *>(
        static_cast<char *>(base_addr) + TYPE_SIZE_OFFSET));

    if (magic.load() != MAGIC_NUMBER) {
      throw std::runtime_error("Invalid magic number in shared memory");
    }

    if (version.load() != VERSION) {
      throw std::runtime_error("Incompatible version in shared memory");
    }

    if (type_size.load() != sizeof(T)) {
      throw std::runtime_error("Type size mismatch in shared memory");
    }

    buffer_size_ = buffer_size.load();

    // Connect to the shared ring buffer
    // For now, this is a placeholder - we'd need to adapt RingBuffer for shared
    // memory
    ring_buffer_ = std::make_unique<nano_stream::RingBuffer<T>>(
        nano_stream::RingBuffer<T>::createSingleProducer(buffer_size_,
                                                         []() { return T{}; }));
  }
};

/**
 * Convenience class that combines IpcSubscription with automatic polling.
 */
template <typename T> class PollingIpcSubscription {
public:
  using event_handler_t = std::function<void(const T &)>;

  /**
   * Create a polling subscription.
   */
  PollingIpcSubscription(const std::string &channel_name,
                         event_handler_t handler)
      : subscription_(channel_name), handler_(std::move(handler)),
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
        int processed =
            subscription_.poll([this](const T &event, int64_t sequence,
                                      bool end_of_batch) { handler_(event); },
                               100);

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
  IpcSubscription<T> &get_subscription() { return subscription_; }

  ~PollingIpcSubscription() { stop_polling(); }

private:
  IpcSubscription<T> subscription_;
  event_handler_t handler_;
  std::atomic<bool> running_;
  std::thread polling_thread_;
};

} // namespace ipc
} // namespace aeron
