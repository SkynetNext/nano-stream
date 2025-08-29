#pragma once

#include "atomic_buffer.h"
#include <cstdint>
#include <memory>

namespace aeron {
namespace concurrent {

/**
 * Simple broadcast receiver for Aeron communication.
 * This is a simplified implementation for the nano-stream project.
 */
class BroadcastReceiver {
public:
  /**
   * Constructor.
   */
  explicit BroadcastReceiver(AtomicBuffer &buffer)
      : buffer_(buffer), capacity_(buffer.capacity()) {}

  /**
   * Destructor.
   */
  ~BroadcastReceiver() = default;

  /**
   * Check if there are any new messages.
   */
  bool receive_next() { return false; }

  /**
   * Get the message type.
   */
  std::int32_t type_id() const { return 0; }

  /**
   * Get the message length.
   */
  std::int32_t length() const { return 0; }

  /**
   * Get the message offset.
   */
  std::int32_t offset() const { return 0; }

  /**
   * Get the buffer.
   */
  AtomicBuffer &buffer() { return buffer_; }

private:
  AtomicBuffer &buffer_;
  std::int32_t capacity_;
};

} // namespace concurrent
} // namespace aeron
