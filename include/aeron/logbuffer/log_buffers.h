#pragma once

#include "aeron/logbuffer/log_buffer_descriptor.h"
#include "aeron/util/memory_mapped_file.h"
#include <memory>
#include <vector>

namespace aeron {
namespace logbuffer {

/**
 * Takes a log file name and maps the file into memory and wraps it with buffers
 * as appropriate.
 *
 * This class manages the memory-mapped log file and provides access to the term
 * buffers and metadata buffer. It follows the same layout as the Java Aeron
 * implementation.
 */
class LogBuffers {
public:
  /**
   * Construct the log buffers for a given log file.
   *
   * @param logFileName to be mapped
   * @param preTouch whether to pre-touch the memory pages
   */
  explicit LogBuffers(const std::string &logFileName, bool preTouch = false);

  /**
   * Construct log buffers from existing memory.
   *
   * @param address base address of the log buffer
   * @param logLength total length of the log buffer
   * @param termLength length of each term
   */
  LogBuffers(std::uint8_t *address, std::int64_t logLength,
             std::int32_t termLength);

  /**
   * Destructor.
   */
  ~LogBuffers();

  // Non-copyable
  LogBuffers(const LogBuffers &) = delete;
  LogBuffers &operator=(const LogBuffers &) = delete;

  // Movable
  LogBuffers(LogBuffers &&other) noexcept;
  LogBuffers &operator=(LogBuffers &&other) noexcept;

  /**
   * Get the atomic buffer for a given index.
   *
   * @param index buffer index (0-3, where 3 is metadata)
   * @return reference to the atomic buffer
   */
  std::uint8_t *buffer(int index) const;

  /**
   * Get the term length.
   */
  int termLength() const { return termLength_; }

  /**
   * Get the page size.
   */
  int pageSize() const { return pageSize_; }

  /**
   * Get the initial term id.
   */
  int initialTermId() const { return initialTermId_; }

  /**
   * Get the MTU length.
   */
  int mtuLength() const { return mtuLength_; }

  /**
   * Get the correlation id.
   */
  std::int64_t correlationId() const { return correlationId_; }

  /**
   * Get the log metadata buffer.
   */
  std::uint8_t *logMetaDataBuffer() const;

  /**
   * Check if the log buffers are valid.
   */
  bool isValid() const {
    return memoryMappedFile_ && memoryMappedFile_->is_valid();
  }

  /**
   * Get the total log length.
   */
  std::int64_t logLength() const { return logLength_; }

  /**
   * Force synchronization to disk.
   */
  void sync();

private:
  void initializeBuffers();
  void preTouchPages();

  std::unique_ptr<util::MemoryMappedFile> memoryMappedFile_;
  std::uint8_t *basePtr_;
  std::int64_t logLength_;
  std::int32_t termLength_;
  std::int32_t pageSize_;
  std::int32_t initialTermId_;
  std::int32_t mtuLength_;
  std::int64_t correlationId_;

  // Buffer pointers for each partition + metadata
  std::vector<std::uint8_t *> buffers_;
};

} // namespace logbuffer
} // namespace aeron
