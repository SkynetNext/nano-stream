#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#endif

namespace aeron {
namespace util {

/**
 * Cross-platform memory-mapped file implementation for Aeron IPC.
 * Provides shared memory capabilities for inter-process communication.
 */
class MemoryMappedFile {
public:
  /**
   * Create a new memory-mapped file.
   *
   * @param filename Path to the file
   * @param size Size in bytes
   * @param create_new Whether to create a new file or open existing
   */
  MemoryMappedFile(const std::string &filename, std::size_t size,
                   bool create_new = true);

  /**
   * Destructor - unmaps and closes the file.
   */
  ~MemoryMappedFile();

  // Non-copyable
  MemoryMappedFile(const MemoryMappedFile &) = delete;
  MemoryMappedFile &operator=(const MemoryMappedFile &) = delete;

  // Movable
  MemoryMappedFile(MemoryMappedFile &&other) noexcept;
  MemoryMappedFile &operator=(MemoryMappedFile &&other) noexcept;

  /**
   * Get the mapped memory address.
   */
  std::uint8_t *memory() const noexcept { return memory_; }

  /**
   * Get the size of the mapped region.
   */
  std::size_t size() const noexcept { return size_; }

  /**
   * Check if the mapping is valid.
   */
  bool is_valid() const noexcept { return memory_ != nullptr; }

  /**
   * Get the filename.
   */
  const std::string &filename() const noexcept { return filename_; }

  /**
   * Force synchronization to disk.
   */
  void sync();

private:
  void cleanup();

  std::string filename_;
  std::size_t size_;
  std::uint8_t *memory_;

#ifdef _WIN32
  HANDLE file_handle_;
  HANDLE mapping_handle_;
#else
  int file_descriptor_;
#endif
};

/**
 * Atomic operations on shared memory.
 */
template <typename T> class SharedAtomic {
public:
  explicit SharedAtomic(T *ptr) : ptr_(ptr) {}

  T load(std::memory_order order = std::memory_order_seq_cst) const {
    return reinterpret_cast<std::atomic<T> *>(ptr_)->load(order);
  }

  void store(T value, std::memory_order order = std::memory_order_seq_cst) {
    reinterpret_cast<std::atomic<T> *>(ptr_)->store(value, order);
  }

  T fetch_add(T value, std::memory_order order = std::memory_order_seq_cst) {
    return reinterpret_cast<std::atomic<T> *>(ptr_)->fetch_add(value, order);
  }

  bool
  compare_exchange_weak(T &expected, T desired,
                        std::memory_order order = std::memory_order_seq_cst) {
    return reinterpret_cast<std::atomic<T> *>(ptr_)->compare_exchange_weak(
        expected, desired, order);
  }

private:
  T *ptr_;
};

} // namespace util
} // namespace aeron
