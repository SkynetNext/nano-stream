#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fileapi.h>
#include <handleapi.h>
#include <memory>
#include <memoryapi.h>
#include <minwindef.h>
#include <stdexcept>
#include <string>
#include <utility>
#include <winnt.h>

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
 * Cross-platform memory mapped file implementation.
 * Header-only with if constexpr for platform handling.
 */
class MemoryMappedFile {
public:
  /**
   * Create or open a memory mapped file.
   */
  MemoryMappedFile(const std::string &path, size_t size, bool create_new = true)
      : path_(path), size_(size), memory_(nullptr) {

    if constexpr (is_windows()) {
      init_windows(create_new);
    } else {
      init_posix(create_new);
    }
  }

  /**
   * Destructor - unmaps the memory and closes file handles.
   */
  ~MemoryMappedFile() { cleanup(); }

  // Non-copyable
  MemoryMappedFile(const MemoryMappedFile &) = delete;
  MemoryMappedFile &operator=(const MemoryMappedFile &) = delete;

  // Movable
  MemoryMappedFile(MemoryMappedFile &&other) noexcept
      : path_(std::move(other.path_)), size_(other.size_),
        memory_(other.memory_) {
    if constexpr (is_windows()) {
      file_handle_ = other.file_handle_;
      mapping_handle_ = other.mapping_handle_;
      other.file_handle_ = INVALID_HANDLE_VALUE;
      other.mapping_handle_ = nullptr;
    } else {
      file_descriptor_ = other.file_descriptor_;
      other.file_descriptor_ = -1;
    }
    other.memory_ = nullptr;
    other.size_ = 0;
  }

  MemoryMappedFile &operator=(MemoryMappedFile &&other) noexcept {
    if (this != &other) {
      cleanup();

      path_ = std::move(other.path_);
      size_ = other.size_;
      memory_ = other.memory_;

      if constexpr (is_windows()) {
        file_handle_ = other.file_handle_;
        mapping_handle_ = other.mapping_handle_;
        other.file_handle_ = INVALID_HANDLE_VALUE;
        other.mapping_handle_ = nullptr;
      } else {
        file_descriptor_ = other.file_descriptor_;
        other.file_descriptor_ = -1;
      }

      other.memory_ = nullptr;
      other.size_ = 0;
    }
    return *this;
  }

  /**
   * Get the mapped memory address.
   */
  void *get_address() const { return memory_; }

  /**
   * Get the size of the mapped memory.
   */
  size_t get_size() const { return size_; }

  /**
   * Check if the mapping is valid.
   */
  bool is_valid() const { return memory_ != nullptr; }

  /**
   * Force synchronization of the mapped memory to disk.
   */
  void sync() {
    if (!is_valid())
      return;

    if constexpr (is_windows()) {
      FlushViewOfFile(memory_, size_);
      FlushFileBuffers(file_handle_);
    } else {
      msync(memory_, size_, MS_SYNC);
    }
  }

  /**
   * Get the file path.
   */
  const std::string &get_path() const { return path_; }

private:
  std::string path_;
  size_t size_;
  void *memory_;

  // Platform-specific handles stored conditionally
  [[no_unique_address]] union {
    struct {
#ifdef _WIN32
      HANDLE file_handle_;
      HANDLE mapping_handle_;
#endif
    } windows_;
    struct {
#ifndef _WIN32
      int file_descriptor_;
#endif
    } posix_;
  };

  static constexpr bool is_windows() {
#ifdef _WIN32
    return true;
#else
    return false;
#endif
  }

  void init_windows(bool create_new) {
    if constexpr (is_windows()) {
#ifdef _WIN32
      DWORD creation_disposition = create_new ? CREATE_ALWAYS : OPEN_EXISTING;

      windows_.file_handle_ =
          CreateFileA(path_.c_str(), GENERIC_READ | GENERIC_WRITE,
                      FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                      creation_disposition, FILE_ATTRIBUTE_NORMAL, nullptr);

      if (windows_.file_handle_ == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Failed to create/open file: " + path_);
      }

      // Set file size if creating new
      if (create_new) {
        LARGE_INTEGER file_size;
        file_size.QuadPart = static_cast<LONGLONG>(size_);
        if (!SetFilePointerEx(windows_.file_handle_, file_size, nullptr,
                              FILE_BEGIN) ||
            !SetEndOfFile(windows_.file_handle_)) {
          CloseHandle(windows_.file_handle_);
          throw std::runtime_error("Failed to set file size: " + path_);
        }
      }

      windows_.mapping_handle_ =
          CreateFileMappingA(windows_.file_handle_, nullptr, PAGE_READWRITE, 0,
                             static_cast<DWORD>(size_), nullptr);

      if (windows_.mapping_handle_ == nullptr) {
        CloseHandle(windows_.file_handle_);
        throw std::runtime_error("Failed to create file mapping: " + path_);
      }

      memory_ = MapViewOfFile(windows_.mapping_handle_, FILE_MAP_ALL_ACCESS, 0,
                              0, size_);

      if (memory_ == nullptr) {
        CloseHandle(windows_.mapping_handle_);
        CloseHandle(windows_.file_handle_);
        throw std::runtime_error("Failed to map view of file: " + path_);
      }
#endif
    }
  }

  void init_posix(bool create_new) {
    if constexpr (!is_windows()) {
#ifndef _WIN32
      int flags = O_RDWR;
      if (create_new) {
        flags |= O_CREAT | O_TRUNC;
      }

      posix_.file_descriptor_ = open(path_.c_str(), flags, 0644);
      if (posix_.file_descriptor_ == -1) {
        throw std::runtime_error("Failed to open file: " + path_ + " - " +
                                 std::strerror(errno));
      }

      // Set file size if creating new
      if (create_new) {
        if (ftruncate(posix_.file_descriptor_, static_cast<off_t>(size_)) ==
            -1) {
          close(posix_.file_descriptor_);
          throw std::runtime_error("Failed to set file size: " + path_ + " - " +
                                   std::strerror(errno));
        }
      }

      memory_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED,
                     posix_.file_descriptor_, 0);
      if (memory_ == MAP_FAILED) {
        close(posix_.file_descriptor_);
        throw std::runtime_error("Failed to map file: " + path_ + " - " +
                                 std::strerror(errno));
      }
#endif
    }
  }

  void cleanup() {
    if (memory_ != nullptr) {
      if constexpr (is_windows()) {
#ifdef _WIN32
        UnmapViewOfFile(memory_);
#endif
      } else {
#ifndef _WIN32
        munmap(memory_, size_);
#endif
      }
      memory_ = nullptr;
    }

    if constexpr (is_windows()) {
#ifdef _WIN32
      if (windows_.mapping_handle_ != nullptr) {
        CloseHandle(windows_.mapping_handle_);
        windows_.mapping_handle_ = nullptr;
      }

      if (windows_.file_handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(windows_.file_handle_);
        windows_.file_handle_ = INVALID_HANDLE_VALUE;
      }
#endif
    } else {
#ifndef _WIN32
      if (posix_.file_descriptor_ != -1) {
        close(posix_.file_descriptor_);
        posix_.file_descriptor_ = -1;
      }
#endif
    }
  }

  // Convenience accessors for platform-specific handles
  auto &file_handle_ = windows_.file_handle_;
  auto &mapping_handle_ = windows_.mapping_handle_;
  auto &file_descriptor_ = posix_.file_descriptor_;
};

/**
 * RAII wrapper for atomic memory operations in shared memory.
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
