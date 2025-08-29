#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#include <winnt.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace aeron {
namespace util {

/**
 * Memory-mapped file abstraction for cross-platform support.
 * Based on Aeron's C++ implementation for IPC communication.
 */
class MemoryMappedFile {
public:
  /**
   * Create a memory mapped file with the specified size.
   */
  static std::unique_ptr<MemoryMappedFile>
  create(const std::string &filename, std::size_t size, bool pre_touch = false);

  /**
   * Map an existing file.
   */
  static std::unique_ptr<MemoryMappedFile> map(const std::string &filename,
                                               bool read_only = false);

  /**
   * Constructor for creating a new file.
   */
  MemoryMappedFile(const std::string &filename, std::size_t size,
                   bool pre_touch);

  /**
   * Constructor for mapping existing file.
   */
  MemoryMappedFile(const std::string &filename, bool read_only);

  /**
   * Destructor - unmaps the file.
   */
  ~MemoryMappedFile();

  // Non-copyable
  MemoryMappedFile(const MemoryMappedFile &) = delete;
  MemoryMappedFile &operator=(const MemoryMappedFile &) = delete;

  // Movable
  MemoryMappedFile(MemoryMappedFile &&other) noexcept;
  MemoryMappedFile &operator=(MemoryMappedFile &&other) noexcept;

  /**
   * Get the memory address of the mapped file.
   */
  std::uint8_t *memory() const noexcept { return memory_; }

  /**
   * Get the size of the mapped file.
   */
  std::size_t size() const noexcept { return size_; }

  /**
   * Get the filename.
   */
  const std::string &filename() const noexcept { return filename_; }

  /**
   * Force sync to disk.
   */
  void sync();

  /**
   * Check if the mapping is valid.
   */
  bool is_valid() const noexcept { return memory_ != nullptr; }

private:
  void cleanup();
  void do_map();
  void pre_touch_pages();

  std::string filename_;
  std::size_t size_;
  std::uint8_t *memory_;
  bool read_only_;
  bool should_unlink_;

#ifdef _WIN32
  HANDLE file_handle_;
  HANDLE mapping_handle_;
#else
  int file_descriptor_;
#endif
};

/**
 * RAII wrapper for memory mapped files that automatically unmaps on
 * destruction.
 */
class MappedFileRAII {
public:
  explicit MappedFileRAII(std::unique_ptr<MemoryMappedFile> file)
      : file_(std::move(file)) {}

  ~MappedFileRAII() = default;

  // Non-copyable, movable
  MappedFileRAII(const MappedFileRAII &) = delete;
  MappedFileRAII &operator=(const MappedFileRAII &) = delete;
  MappedFileRAII(MappedFileRAII &&) = default;
  MappedFileRAII &operator=(MappedFileRAII &&) = default;

  MemoryMappedFile *get() const { return file_.get(); }
  MemoryMappedFile *operator->() const { return file_.get(); }
  MemoryMappedFile &operator*() const { return *file_; }

private:
  std::unique_ptr<MemoryMappedFile> file_;
};

} // namespace util
} // namespace aeron