#pragma once

#include "../../util/memory_mapped_file.h"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace aeron {
namespace driver {

/**
 * Counter types for monitoring and statistics.
 */
enum class CounterType : std::int32_t {
  SYSTEM_COUNTER_TYPE_ID = 0,
  BYTES_SENT = 1,
  BYTES_RECEIVED = 2,
  RECEIVER_HWM = 3,
  RECEIVER_POS = 4,
  SEND_CHANNEL_STATUS = 5,
  RECEIVE_CHANNEL_STATUS = 6,
  SENDER_LIMIT = 7,
  PER_IMAGE_TYPE_ID = 8,
  PUBLISHER_LIMIT = 9,
  SENDER_POSITION = 10,
  PUBLISHER_POSITION = 11,
  RECEIVER_POSITION = 12,
  SUBSCRIPTION_POSITION = 13,
  CLIENT_HEARTBEAT_TIMESTAMP = 14,
  ERRORS = 15,
  UNBLOCKED_PUBLICATIONS = 16,
  UNBLOCKED_CONTROL_COMMANDS = 17,
  POSSIBLE_TTL_ASYMMETRY = 18,
  CONTROLLABLE_IDLE_STRATEGY = 19,
  LOSS_GAP_FILLS = 20,
  CLIENT_TIMEOUTS = 21,
  CONDUCTOR_MAX_CYCLE_TIME = 22,
  CONDUCTOR_CYCLE_TIME_THRESHOLD_EXCEEDED = 23,
  SENDER_MAX_CYCLE_TIME = 24,
  SENDER_CYCLE_TIME_THRESHOLD_EXCEEDED = 25,
  RECEIVER_MAX_CYCLE_TIME = 26,
  RECEIVER_CYCLE_TIME_THRESHOLD_EXCEEDED = 27,
  NAME_RESOLVER_NEIGHBORS_COUNTER_TYPE_ID = 28,
  NAME_RESOLVER_CACHE_ENTRIES_COUNTER_TYPE_ID = 29,
  FLOW_CONTROL_UNDER_RUNS = 30,
  FLOW_CONTROL_OVER_RUNS = 31
};

/**
 * Counter metadata stored in shared memory.
 */
struct alignas(64) CounterMetadata {
  std::int32_t state;           // Counter state (allocated, active, etc.)
  std::int32_t type_id;         // Counter type
  std::int64_t registration_id; // Registration ID
  std::int64_t owner_id;        // Owner client ID
  std::int32_t reference_count; // Reference count
  std::int32_t label_length;    // Length of label string
  char label[384];              // Counter label/description

  static constexpr std::int32_t RECORD_ALLOCATED = 1;
  static constexpr std::int32_t RECORD_RECLAIMED = -1;
  static constexpr std::int32_t RECORD_UNUSED = 0;
};

/**
 * Counter value stored in shared memory.
 */
struct alignas(64) CounterValue {
  std::int64_t value;
  char padding[56]; // Pad to cache line size
};

/**
 * Manages counters for monitoring and statistics.
 * Counters are stored in shared memory for visibility to external tools.
 */
class CountersManager {
public:
  /**
   * Constructor.
   *
   * @param metadata_buffer Shared memory for counter metadata
   * @param values_buffer Shared memory for counter values
   * @param metadata_length Length of metadata buffer
   * @param values_length Length of values buffer
   */
  CountersManager(std::uint8_t *metadata_buffer, std::uint8_t *values_buffer,
                  std::size_t metadata_length, std::size_t values_length);

  /**
   * Destructor.
   */
  ~CountersManager();

  /**
   * Allocate a new counter.
   *
   * @param type_id Counter type
   * @param label Counter label/description
   * @param registration_id Registration ID (optional)
   * @param owner_id Owner client ID (optional)
   * @return Counter ID, or -1 if allocation failed
   */
  std::int32_t allocate(CounterType type_id, const std::string &label,
                        std::int64_t registration_id = 0,
                        std::int64_t owner_id = 0);

  /**
   * Free a counter.
   *
   * @param counter_id Counter ID to free
   */
  void free(std::int32_t counter_id);

  /**
   * Get counter value.
   *
   * @param counter_id Counter ID
   * @return Current counter value
   */
  std::int64_t get_counter_value(std::int32_t counter_id) const;

  /**
   * Set counter value.
   *
   * @param counter_id Counter ID
   * @param value New value
   */
  void set_counter_value(std::int32_t counter_id, std::int64_t value);

  /**
   * Increment counter value.
   *
   * @param counter_id Counter ID
   * @param increment Amount to add
   * @return New counter value
   */
  std::int64_t increment_counter(std::int32_t counter_id,
                                 std::int64_t increment = 1);

  /**
   * Get counter metadata.
   *
   * @param counter_id Counter ID
   * @return Pointer to metadata, or nullptr if invalid
   */
  const CounterMetadata *get_counter_metadata(std::int32_t counter_id) const;

  /**
   * Get counter label.
   *
   * @param counter_id Counter ID
   * @return Counter label string
   */
  std::string get_counter_label(std::int32_t counter_id) const;

  /**
   * Check if counter is allocated.
   *
   * @param counter_id Counter ID
   * @return true if allocated, false otherwise
   */
  bool is_counter_allocated(std::int32_t counter_id) const;

  /**
   * Get the maximum number of counters.
   */
  std::int32_t max_counter_id() const { return max_counter_id_; }

  /**
   * Iterate over all allocated counters.
   *
   * @param callback Function called for each allocated counter
   */
  void for_each_counter(
      std::function<void(std::int32_t, const CounterMetadata &, std::int64_t)>
          callback) const;

private:
  /**
   * Find next available counter slot.
   */
  std::int32_t find_next_counter_id();

  /**
   * Get metadata pointer for counter ID.
   */
  CounterMetadata *get_metadata_ptr(std::int32_t counter_id);

  /**
   * Get value pointer for counter ID.
   */
  CounterValue *get_value_ptr(std::int32_t counter_id);

  std::uint8_t *metadata_buffer_;
  std::uint8_t *values_buffer_;
  std::size_t metadata_length_;
  std::size_t values_length_;

  std::int32_t max_counter_id_;
  std::atomic<std::int32_t> next_counter_id_{0};

  mutable std::mutex allocation_mutex_;

  static constexpr std::size_t METADATA_RECORD_SIZE = sizeof(CounterMetadata);
  static constexpr std::size_t VALUE_RECORD_SIZE = sizeof(CounterValue);
};

/**
 * RAII wrapper for counter management.
 */
class Counter {
public:
  /**
   * Constructor.
   */
  Counter(CountersManager &manager, std::int32_t counter_id)
      : manager_(manager), counter_id_(counter_id) {}

  /**
   * Destructor - automatically frees the counter.
   */
  ~Counter() {
    if (counter_id_ >= 0) {
      manager_.free(counter_id_);
    }
  }

  // Non-copyable
  Counter(const Counter &) = delete;
  Counter &operator=(const Counter &) = delete;

  // Movable
  Counter(Counter &&other) noexcept
      : manager_(other.manager_), counter_id_(other.counter_id_) {
    other.counter_id_ = -1;
  }

  Counter &operator=(Counter &&other) noexcept {
    if (this != &other) {
      if (counter_id_ >= 0) {
        manager_.free(counter_id_);
      }
      counter_id_ = other.counter_id_;
      other.counter_id_ = -1;
    }
    return *this;
  }

  /**
   * Get counter ID.
   */
  std::int32_t id() const { return counter_id_; }

  /**
   * Get current value.
   */
  std::int64_t get() const { return manager_.get_counter_value(counter_id_); }

  /**
   * Set value.
   */
  void set(std::int64_t value) {
    manager_.set_counter_value(counter_id_, value);
  }

  /**
   * Increment value.
   */
  std::int64_t increment(std::int64_t amount = 1) {
    return manager_.increment_counter(counter_id_, amount);
  }

  /**
   * Increment by 1 (postfix).
   */
  std::int64_t operator++(int) { return increment(); }

  /**
   * Add to counter.
   */
  Counter &operator+=(std::int64_t amount) {
    increment(amount);
    return *this;
  }

private:
  CountersManager &manager_;
  std::int32_t counter_id_;
};

} // namespace driver
} // namespace aeron
