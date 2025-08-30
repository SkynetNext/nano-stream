#include "aeron/util/memory_mapped_file.h"
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#include <io.h>
#else
#include <sys/stat.h>
#endif

namespace aeron {
namespace util {

MemoryMappedFile::MemoryMappedFile(const std::string &filename,
                                   std::size_t size, bool create_new)
    : filename_(filename), size_(size), memory_(nullptr)
#ifdef _WIN32
      ,
      file_handle_(INVALID_HANDLE_VALUE), mapping_handle_(INVALID_HANDLE_VALUE)
#else
      ,
      file_descriptor_(-1)
#endif
{
#ifdef _WIN32
  DWORD creation_disposition = create_new ? CREATE_ALWAYS : OPEN_EXISTING;
  DWORD access = GENERIC_READ | GENERIC_WRITE;

  file_handle_ = CreateFileA(
      filename.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
      creation_disposition, FILE_ATTRIBUTE_NORMAL, nullptr);

  if (file_handle_ == INVALID_HANDLE_VALUE) {
    throw std::runtime_error("Failed to create/open file: " + filename);
  }

  if (create_new) {
    LARGE_INTEGER file_size;
    file_size.QuadPart = size;
    if (!SetFilePointerEx(file_handle_, file_size, nullptr, FILE_BEGIN) ||
        !SetEndOfFile(file_handle_)) {
      cleanup();
      throw std::runtime_error("Failed to set file size: " + filename);
    }
  } else {
    LARGE_INTEGER file_size;
    if (!GetFileSizeEx(file_handle_, &file_size)) {
      cleanup();
      throw std::runtime_error("Failed to get file size: " + filename);
    }
    size_ = static_cast<std::size_t>(file_size.QuadPart);
  }

  mapping_handle_ =
      CreateFileMappingA(file_handle_, nullptr, PAGE_READWRITE, 0, 0, nullptr);

  if (mapping_handle_ == nullptr) {
    cleanup();
    throw std::runtime_error("Failed to create file mapping: " + filename);
  }

  memory_ = static_cast<std::uint8_t *>(
      MapViewOfFile(mapping_handle_, FILE_MAP_ALL_ACCESS, 0, 0, size_));

  if (memory_ == nullptr) {
    cleanup();
    throw std::runtime_error("Failed to map view of file: " + filename);
  }

#else // POSIX
  int flags = O_RDWR;
  if (create_new) {
    flags |= O_CREAT | O_TRUNC;
  }

  file_descriptor_ = open(filename.c_str(), flags, 0644);
  if (file_descriptor_ == -1) {
    throw std::runtime_error("Failed to open file: " + filename);
  }

  if (create_new) {
    if (ftruncate(file_descriptor_, size) == -1) {
      cleanup();
      throw std::runtime_error("Failed to set file size: " + filename);
    }
  } else {
    struct stat st;
    if (fstat(file_descriptor_, &st) == -1) {
      cleanup();
      throw std::runtime_error("Failed to get file size: " + filename);
    }
    size_ = st.st_size;
  }

  memory_ = static_cast<std::uint8_t *>(mmap(
      nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor_, 0));

  if (memory_ == MAP_FAILED) {
    memory_ = nullptr;
    cleanup();
    throw std::runtime_error("Failed to map file: " + filename);
  }
#endif
}

MemoryMappedFile::~MemoryMappedFile() {
  try {
    cleanup();
  } catch (...) {
    // Ignore cleanup errors during destruction
    // This prevents exceptions from being thrown during object destruction
  }
}

MemoryMappedFile::MemoryMappedFile(MemoryMappedFile &&other) noexcept
    : filename_(std::move(other.filename_)), size_(other.size_),
      memory_(other.memory_)
#ifdef _WIN32
      ,
      file_handle_(other.file_handle_), mapping_handle_(other.mapping_handle_)
#else
      ,
      file_descriptor_(other.file_descriptor_)
#endif
{
  other.memory_ = nullptr;
  other.size_ = 0;
#ifdef _WIN32
  other.file_handle_ = INVALID_HANDLE_VALUE;
  other.mapping_handle_ = INVALID_HANDLE_VALUE;
#else
  other.file_descriptor_ = -1;
#endif
}

MemoryMappedFile &
MemoryMappedFile::operator=(MemoryMappedFile &&other) noexcept {
  if (this != &other) {
    cleanup();

    filename_ = std::move(other.filename_);
    size_ = other.size_;
    memory_ = other.memory_;
#ifdef _WIN32
    file_handle_ = other.file_handle_;
    mapping_handle_ = other.mapping_handle_;
#else
    file_descriptor_ = other.file_descriptor_;
#endif

    other.memory_ = nullptr;
    other.size_ = 0;
#ifdef _WIN32
    other.file_handle_ = INVALID_HANDLE_VALUE;
    other.mapping_handle_ = INVALID_HANDLE_VALUE;
#else
    other.file_descriptor_ = -1;
#endif
  }
  return *this;
}

void MemoryMappedFile::sync() {
#ifdef _WIN32
  if (memory_) {
    FlushViewOfFile(memory_, size_);
    FlushFileBuffers(file_handle_);
  }
#else
  if (memory_) {
    msync(memory_, size_, MS_SYNC);
  }
#endif
}

void MemoryMappedFile::cleanup() {
#ifdef _WIN32
  if (memory_) {
    UnmapViewOfFile(memory_);
    memory_ = nullptr;
  }
  if (mapping_handle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(mapping_handle_);
    mapping_handle_ = INVALID_HANDLE_VALUE;
  }
  if (file_handle_ != INVALID_HANDLE_VALUE) {
    CloseHandle(file_handle_);
    file_handle_ = INVALID_HANDLE_VALUE;
  }
#else
  if (memory_) {
    munmap(memory_, size_);
    memory_ = nullptr;
  }
  if (file_descriptor_ != -1) {
    close(file_descriptor_);
    file_descriptor_ = -1;
  }
#endif
}

} // namespace util
} // namespace aeron
