#pragma once

#include "concurrent/atomic_buffer.h"
#include "concurrent/logbuffer/log_buffer_descriptor.h"
#include "util/memory_mapped_file.h"
#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace aeron {

/**
 * Log buffers container for term buffers and metadata.
 * Manages the memory layout of Aeron's triple-buffered log structure.
 */
class LogBuffers {
public:
  /**
   * Create log buffers for a new stream.
   */
  static std::unique_ptr<LogBuffers> create(const std::string &log_file_name,
                                            std::int32_t term_length,
                                            std::int32_t file_page_size,
                                            std::int32_t correlation_id);

  /**
   * Map existing log buffers.
   */
  static std::unique_ptr<LogBuffers> map(const std::string &log_file_name);

  /**
   * Map existing log buffers with read-only access.
   */
  static std::unique_ptr<LogBuffers>
  map_read_only(const std::string &log_file_name);

  /**
   * Constructor for LogBuffers.
   */
  LogBuffers(std::unique_ptr<util::MemoryMappedFile> mapped_file,
             concurrent::AtomicBuffer log_meta_data_buffer,
             std::array<concurrent::AtomicBuffer, 3> term_buffers) noexcept;

  /**
   * Get the log metadata buffer.
   */
  concurrent::AtomicBuffer &log_meta_data_buffer() noexcept {
    return log_meta_data_buffer_;
  }

  /**
   * Get the log metadata buffer (const version).
   */
  const concurrent::AtomicBuffer &log_meta_data_buffer() const noexcept {
    return log_meta_data_buffer_;
  }

  /**
   * Get the term buffers array.
   */
  std::array<concurrent::AtomicBuffer, 3> &term_buffers() noexcept {
    return term_buffers_;
  }

  /**
   * Get the term buffers array (const version).
   */
  const std::array<concurrent::AtomicBuffer, 3> &term_buffers() const noexcept {
    return term_buffers_;
  }

  /**
   * Get a specific term buffer by index.
   */
  concurrent::AtomicBuffer &term_buffer(int index) noexcept {
    return term_buffers_[index];
  }

  /**
   * Get a specific term buffer by index (const version).
   */
  const concurrent::AtomicBuffer &term_buffer(int index) const noexcept {
    return term_buffers_[index];
  }

  /**
   * Get the memory mapped file.
   */
  util::MemoryMappedFile &mapped_file() noexcept { return *mapped_file_; }

  /**
   * Get the memory mapped file (const version).
   */
  const util::MemoryMappedFile &mapped_file() const noexcept {
    return *mapped_file_;
  }

  /**
   * Get the term length.
   */
  std::int32_t term_length() const noexcept {
    return concurrent::logbuffer::LogBufferDescriptor::term_length(
        log_meta_data_buffer_);
  }

  /**
   * Get the page size.
   */
  std::int32_t page_size() const noexcept {
    return concurrent::logbuffer::LogBufferDescriptor::page_size(
        log_meta_data_buffer_);
  }

  /**
   * Get the MTU length.
   */
  std::int32_t mtu_length() const noexcept {
    return concurrent::logbuffer::LogBufferDescriptor::mtu_length(
        log_meta_data_buffer_);
  }

  /**
   * Get the initial term id.
   */
  std::int32_t initial_term_id() const noexcept {
    return concurrent::logbuffer::LogBufferDescriptor::initial_term_id(
        log_meta_data_buffer_);
  }

  /**
   * Get the correlation id.
   */
  std::int64_t correlation_id() const noexcept {
    return log_meta_data_buffer_.get_int64(
        concurrent::logbuffer::LogBufferDescriptor::LOG_INITIAL_TERM_ID_OFFSET +
        sizeof(std::int64_t));
  }

  /**
   * Close the log buffers and release resources.
   */
  void close() noexcept;

  /**
   * Check if the log buffers are closed.
   */
  bool is_closed() const noexcept { return mapped_file_ == nullptr; }

  /**
   * Get the file length needed for a given term length.
   */
  static std::int64_t compute_log_length(std::int32_t term_length,
                                         std::int32_t file_page_size) noexcept;

  /**
   * Check that term length is valid.
   */
  static void check_term_length(std::int32_t term_length);

  /**
   * Check that file page size is valid.
   */
  static void check_page_size(std::int32_t file_page_size);

private:
  std::unique_ptr<util::MemoryMappedFile> mapped_file_;
  concurrent::AtomicBuffer log_meta_data_buffer_;
  std::array<concurrent::AtomicBuffer, 3> term_buffers_;

  /**
   * Initialize log metadata.
   */
  void initialize_log_metadata(std::int32_t term_length,
                               std::int32_t file_page_size,
                               std::int32_t correlation_id) noexcept;

  /**
   * Map term buffers from the memory mapped file.
   */
  static std::array<concurrent::AtomicBuffer, 3>
  map_term_buffers(util::MemoryMappedFile &mapped_file,
                   std::int32_t term_length) noexcept;

  /**
   * Map log metadata buffer from the memory mapped file.
   */
  static concurrent::AtomicBuffer
  map_log_meta_data_buffer(util::MemoryMappedFile &mapped_file,
                           std::int32_t term_length) noexcept;
};

/**
 * Factory for creating log file names.
 */
namespace log_file_name {

/**
 * Generate a log file name for a stream.
 */
std::string create(const std::string &aeron_dir, std::int32_t session_id,
                   std::int32_t stream_id, std::int64_t correlation_id);

/**
 * Generate a publication log file name.
 */
std::string create_publication(const std::string &aeron_dir,
                               std::int32_t session_id, std::int32_t stream_id,
                               std::int64_t correlation_id);

/**
 * Generate a subscription log file name.
 */
std::string create_subscription(const std::string &aeron_dir,
                                std::int64_t correlation_id);

} // namespace log_file_name

} // namespace aeron
