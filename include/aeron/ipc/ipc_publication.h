#pragma once

#include "../../nano_stream/ring_buffer.h"
#include "../../nano_stream/sequence.h"
#include "../util/memory_mapped_file.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace aeron {
namespace ipc {

/**
 * IPC Publication for inter-process communication using shared memory.
 * Based on nano_stream::RingBuffer but designed for cross-process usage.
 */
template <typename T> class IpcPublication {
public:
  /**
   * Create an IPC publication.
   *
   * @param channel_name Unique name for the IPC channel
   * @param buffer_size Size of the ring buffer (must be power of 2)
   * @param factory Function to create event instances
   */
  template <typename FactoryFn>
  IpcPublication(const std::string &channel_name, size_t buffer_size,
                 FactoryFn &&factory)
      : channel_name_(channel_name), buffer_size_(buffer_size),
        shared_memory_size_(calculate_shared_memory_size(buffer_size)),
        memory_map_(get_shared_memory_path(channel_name), shared_memory_size_,
                    true) {
    if (!memory_map_.is_valid()) {
      throw std::runtime_error("Failed to create shared memory for channel: " +
                               channel_name);
    }

    // Initialize shared memory layout
    init_shared_memory(std::forward<FactoryFn>(factory));
  }

  /**
   * Connect to existing IPC publication.
   */
  IpcPublication(const std::string &channel_name)
      : channel_name_(channel_name),
        memory_map_(get_shared_memory_path(channel_name), 0, false) {
    if (!memory_map_.is_valid()) {
      throw std::runtime_error(
          "Failed to connect to shared memory for channel: " + channel_name);
    }

    // Read header to get buffer size
    connect_to_existing();
  }

  /**
   * Publish a single event.
   *
   * @param data The data to publish
   * @return true if published successfully, false if insufficient capacity
   */
  bool offer(const T &data) {
    if constexpr (std::is_trivially_copyable_v<T>) {
      auto result = ring_buffer_->try_next();
      if (result.has_value()) {
        int64_t sequence = result.value();
        T &event = ring_buffer_->get(sequence);
        event = data;
        ring_buffer_->publish(sequence);
        return true;
      }
      return false;
    } else {
      static_assert(std::is_trivially_copyable_v<T>,
                    "IPC Publication requires trivially copyable types");
    }
  }

  /**
   * Try to claim space for n events.
   *
   * @param n Number of events to claim
   * @return Starting sequence if successful, nullopt otherwise
   */
  std::optional<int64_t> try_claim(int n = 1) {
    return ring_buffer_->try_next(n);
  }

  /**
   * Get event at the specified sequence.
   */
  T &get(int64_t sequence) { return ring_buffer_->get(sequence); }

  /**
   * Publish the claimed sequence(s).
   */
  void publish(int64_t sequence) { ring_buffer_->publish(sequence); }

  /**
   * Publish a range of sequences.
   */
  void publish(int64_t start_sequence, int64_t end_sequence) {
    for (int64_t seq = start_sequence; seq <= end_sequence; ++seq) {
      ring_buffer_->publish(seq);
    }
  }

  /**
   * Check if the publication is connected and ready.
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
   * Get remaining capacity.
   */
  size_t remaining_capacity() {
    return ring_buffer_ ? ring_buffer_->remaining_capacity() : 0;
  }

private:
  std::string channel_name_;
  size_t buffer_size_;
  size_t shared_memory_size_;
  util::MemoryMappedFile memory_map_;
  std::unique_ptr<nano_stream::RingBuffer<T>> ring_buffer_;

  // Shared memory layout offsets
  static constexpr size_t HEADER_SIZE = 64; // Cache line aligned
  static constexpr size_t MAGIC_OFFSET = 0;
  static constexpr size_t VERSION_OFFSET = 8;
  static constexpr size_t BUFFER_SIZE_OFFSET = 16;
  static constexpr size_t TYPE_SIZE_OFFSET = 24;
  static constexpr size_t RING_BUFFER_OFFSET = HEADER_SIZE;

  static constexpr uint64_t MAGIC_NUMBER = 0x41455244'4F4E414EULL; // "NANO-AER"
  static constexpr uint32_t VERSION = 1;

  /**
   * Calculate total shared memory size needed.
   */
  static size_t calculate_shared_memory_size(size_t buffer_size) {
    // Header + RingBuffer metadata + Events array
    return HEADER_SIZE + 1024 + // RingBuffer overhead
           buffer_size * sizeof(T);
  }

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
   * Initialize shared memory layout for new publication.
   */
  template <typename FactoryFn> void init_shared_memory(FactoryFn &&factory) {
    void *base_addr = memory_map_.get_address();

    // Write header
    util::SharedAtomic<uint64_t> magic(
        static_cast<uint64_t *>(static_cast<char *>(base_addr) + MAGIC_OFFSET));
    util::SharedAtomic<uint32_t> version(static_cast<uint32_t *>(
        static_cast<char *>(base_addr) + VERSION_OFFSET));
    util::SharedAtomic<uint64_t> buffer_size(static_cast<uint64_t *>(
        static_cast<char *>(base_addr) + BUFFER_SIZE_OFFSET));
    util::SharedAtomic<uint64_t> type_size(static_cast<uint64_t *>(
        static_cast<char *>(base_addr) + TYPE_SIZE_OFFSET));

    magic.store(MAGIC_NUMBER);
    version.store(VERSION);
    buffer_size.store(buffer_size_);
    type_size.store(sizeof(T));

    // Initialize ring buffer in shared memory
    void *ring_buffer_addr =
        static_cast<char *>(base_addr) + RING_BUFFER_OFFSET;

    // For now, create a local ring buffer and copy its layout
    // In a full implementation, we'd need to adapt RingBuffer for shared memory
    ring_buffer_ = std::make_unique<nano_stream::RingBuffer<T>>(
        nano_stream::RingBuffer<T>::createSingleProducer(
            buffer_size_, std::forward<FactoryFn>(factory)));

    memory_map_.sync();
  }

  /**
   * Connect to existing shared memory.
   */
  void connect_to_existing() {
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
    shared_memory_size_ = calculate_shared_memory_size(buffer_size_);

    // Connect to existing ring buffer
    // For now, this is a placeholder - we'd need to adapt RingBuffer for shared
    // memory
    ring_buffer_ = std::make_unique<nano_stream::RingBuffer<T>>(
        nano_stream::RingBuffer<T>::createSingleProducer(buffer_size_,
                                                         []() { return T{}; }));
  }
};

} // namespace ipc
} // namespace aeron
