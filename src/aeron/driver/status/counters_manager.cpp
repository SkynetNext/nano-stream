#ifdef _WIN32
#define NOMINMAX
#endif

#include "aeron/driver/status/counters_manager.h"
#include <algorithm>
#include <cstring>

namespace aeron {
namespace driver {

CountersManager::CountersManager(std::uint8_t *metadata_buffer,
                                 std::uint8_t *values_buffer,
                                 std::size_t metadata_length,
                                 std::size_t values_length)
    : metadata_buffer_(metadata_buffer), values_buffer_(values_buffer),
      metadata_length_(metadata_length), values_length_(values_length) {

  max_counter_id_ = std::min(
      static_cast<std::int32_t>(metadata_length / METADATA_RECORD_SIZE),
      static_cast<std::int32_t>(values_length / VALUE_RECORD_SIZE));

  // Initialize all counters as unused
  for (std::int32_t i = 0; i < max_counter_id_; ++i) {
    CounterMetadata *metadata = get_metadata_ptr(i);
    metadata->state = CounterMetadata::RECORD_UNUSED;
    metadata->type_id = 0;
    metadata->registration_id = 0;
    metadata->owner_id = 0;
    metadata->reference_count = 0;
    metadata->label_length = 0;
    std::memset(metadata->label, 0, sizeof(metadata->label));

    CounterValue *value = get_value_ptr(i);
    value->value = 0;
  }
}

CountersManager::~CountersManager() {
  // Nothing to clean up - we don't own the buffers
}

std::int32_t CountersManager::allocate(CounterType type_id,
                                       const std::string &label,
                                       std::int64_t registration_id,
                                       std::int64_t owner_id) {
  std::lock_guard<std::mutex> lock(allocation_mutex_);

  std::int32_t counter_id = find_next_counter_id();
  if (counter_id == -1) {
    return -1; // No available counters
  }

  CounterMetadata *metadata = get_metadata_ptr(counter_id);
  CounterValue *value = get_value_ptr(counter_id);

  // Initialize metadata
  metadata->state = CounterMetadata::RECORD_ALLOCATED;
  metadata->type_id = static_cast<std::int32_t>(type_id);
  metadata->registration_id = registration_id;
  metadata->owner_id = owner_id;
  metadata->reference_count = 1;
  metadata->label_length =
      std::min(static_cast<std::int32_t>(label.length()),
               static_cast<std::int32_t>(sizeof(metadata->label) - 1));

#ifdef _WIN32
  strncpy_s(metadata->label, sizeof(metadata->label), label.c_str(),
            metadata->label_length);
#else
  std::strncpy(metadata->label, label.c_str(), metadata->label_length);
  metadata->label[metadata->label_length] = '\0';
#endif

  // Initialize value
  value->value = 0;

  return counter_id;
}

void CountersManager::free(std::int32_t counter_id) {
  if (counter_id < 0 || counter_id >= max_counter_id_) {
    return;
  }

  std::lock_guard<std::mutex> lock(allocation_mutex_);

  CounterMetadata *metadata = get_metadata_ptr(counter_id);
  if (metadata->state == CounterMetadata::RECORD_ALLOCATED) {
    metadata->reference_count--;
    if (metadata->reference_count <= 0) {
      metadata->state = CounterMetadata::RECORD_RECLAIMED;

      // Clear the value
      CounterValue *value = get_value_ptr(counter_id);
      value->value = 0;
    }
  }
}

std::int64_t CountersManager::get_counter_value(std::int32_t counter_id) const {
  if (counter_id < 0 || counter_id >= max_counter_id_) {
    return 0;
  }

  const CounterValue *value =
      const_cast<CountersManager *>(this)->get_value_ptr(counter_id);
  return value->value;
}

void CountersManager::set_counter_value(std::int32_t counter_id,
                                        std::int64_t value) {
  if (counter_id < 0 || counter_id >= max_counter_id_) {
    return;
  }

  CounterValue *counter_value = get_value_ptr(counter_id);
  counter_value->value = value;
}

std::int64_t CountersManager::increment_counter(std::int32_t counter_id,
                                                std::int64_t increment) {
  if (counter_id < 0 || counter_id >= max_counter_id_) {
    return 0;
  }

  CounterValue *value = get_value_ptr(counter_id);
  std::int64_t old_value = value->value;
  value->value += increment;
  return value->value;
}

const CounterMetadata *
CountersManager::get_counter_metadata(std::int32_t counter_id) const {
  if (counter_id < 0 || counter_id >= max_counter_id_) {
    return nullptr;
  }

  return const_cast<CountersManager *>(this)->get_metadata_ptr(counter_id);
}

std::string CountersManager::get_counter_label(std::int32_t counter_id) const {
  const CounterMetadata *metadata = get_counter_metadata(counter_id);
  if (metadata && metadata->state == CounterMetadata::RECORD_ALLOCATED) {
    return std::string(metadata->label, metadata->label_length);
  }
  return "";
}

bool CountersManager::is_counter_allocated(std::int32_t counter_id) const {
  const CounterMetadata *metadata = get_counter_metadata(counter_id);
  return metadata && metadata->state == CounterMetadata::RECORD_ALLOCATED;
}

void CountersManager::for_each_counter(
    std::function<void(std::int32_t, const CounterMetadata &, std::int64_t)>
        callback) const {
  for (std::int32_t i = 0; i < max_counter_id_; ++i) {
    const CounterMetadata *metadata = get_counter_metadata(i);
    if (metadata && metadata->state == CounterMetadata::RECORD_ALLOCATED) {
      std::int64_t value = get_counter_value(i);
      callback(i, *metadata, value);
    }
  }
}

std::int32_t CountersManager::find_next_counter_id() {
  std::int32_t start_id = next_counter_id_.load();

  for (std::int32_t i = 0; i < max_counter_id_; ++i) {
    std::int32_t counter_id = (start_id + i) % max_counter_id_;
    CounterMetadata *metadata = get_metadata_ptr(counter_id);

    if (metadata->state == CounterMetadata::RECORD_UNUSED ||
        metadata->state == CounterMetadata::RECORD_RECLAIMED) {
      next_counter_id_.store((counter_id + 1) % max_counter_id_);
      return counter_id;
    }
  }

  return -1; // No available counters
}

CounterMetadata *CountersManager::get_metadata_ptr(std::int32_t counter_id) {
  return reinterpret_cast<CounterMetadata *>(
      metadata_buffer_ + (counter_id * METADATA_RECORD_SIZE));
}

CounterValue *CountersManager::get_value_ptr(std::int32_t counter_id) {
  return reinterpret_cast<CounterValue *>(values_buffer_ +
                                          (counter_id * VALUE_RECORD_SIZE));
}

} // namespace driver
} // namespace aeron
