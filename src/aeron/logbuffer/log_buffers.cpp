#include "aeron/logbuffer/log_buffers.h"
#include <cstring>
#include <stdexcept>

namespace aeron {
namespace logbuffer {

LogBuffers::LogBuffers(const std::string &logFileName, bool preTouch)
    : basePtr_(nullptr), logLength_(0), termLength_(0), pageSize_(0),
      initialTermId_(0), mtuLength_(0), correlationId_(0) {

  try {
    // Create memory mapped file with default log buffer size
    // Calculate the required size: 3 term buffers + metadata
    const std::int32_t termLength = 64 * 1024; // 64KB per term
    const std::int32_t partitionCount = 3;
    const std::size_t totalSize = (partitionCount * termLength) +
                                  LogBufferDescriptor::LOG_META_DATA_LENGTH;

    memoryMappedFile_ =
        std::make_unique<util::MemoryMappedFile>(logFileName, totalSize, true);

    if (!memoryMappedFile_->is_valid()) {
      throw std::runtime_error("Failed to map log file: " + logFileName);
    }

    basePtr_ = memoryMappedFile_->memory();
    logLength_ = static_cast<std::int64_t>(memoryMappedFile_->size());

    if (logLength_ < LogBufferDescriptor::LOG_META_DATA_LENGTH) {
      throw std::runtime_error(
          "Log file length less than min length of " +
          std::to_string(LogBufferDescriptor::LOG_META_DATA_LENGTH) +
          ": length=" + std::to_string(logLength_));
    }

    // Initialize the log metadata for a newly created file
    initializeNewLogFile(termLength);

    initializeBuffers();

    if (preTouch) {
      preTouchPages();
    }

  } catch (const std::exception &e) {
    throw std::runtime_error("Failed to create LogBuffers: " +
                             std::string(e.what()));
  }
}

LogBuffers::LogBuffers(std::uint8_t *address, std::int64_t logLength,
                       std::int32_t termLength)
    : memoryMappedFile_(nullptr), basePtr_(address), logLength_(logLength),
      termLength_(termLength), pageSize_(0), initialTermId_(0), mtuLength_(0),
      correlationId_(0) {

  initializeBuffers();
}

LogBuffers::~LogBuffers() = default;

LogBuffers::LogBuffers(LogBuffers &&other) noexcept
    : memoryMappedFile_(std::move(other.memoryMappedFile_)),
      basePtr_(other.basePtr_), logLength_(other.logLength_),
      termLength_(other.termLength_), pageSize_(other.pageSize_),
      initialTermId_(other.initialTermId_), mtuLength_(other.mtuLength_),
      correlationId_(other.correlationId_),
      buffers_(std::move(other.buffers_)) {

  other.basePtr_ = nullptr;
  other.logLength_ = 0;
  other.termLength_ = 0;
  other.pageSize_ = 0;
  other.initialTermId_ = 0;
  other.mtuLength_ = 0;
  other.correlationId_ = 0;
}

LogBuffers &LogBuffers::operator=(LogBuffers &&other) noexcept {
  if (this != &other) {
    memoryMappedFile_ = std::move(other.memoryMappedFile_);
    basePtr_ = other.basePtr_;
    logLength_ = other.logLength_;
    termLength_ = other.termLength_;
    pageSize_ = other.pageSize_;
    initialTermId_ = other.initialTermId_;
    mtuLength_ = other.mtuLength_;
    correlationId_ = other.correlationId_;
    buffers_ = std::move(other.buffers_);

    other.basePtr_ = nullptr;
    other.logLength_ = 0;
    other.termLength_ = 0;
    other.pageSize_ = 0;
    other.initialTermId_ = 0;
    other.mtuLength_ = 0;
    other.correlationId_ = 0;
  }
  return *this;
}

std::uint8_t *LogBuffers::buffer(int index) const {
  if (index < 0 || index >= static_cast<int>(buffers_.size())) {
    throw std::out_of_range("Buffer index out of range: " +
                            std::to_string(index));
  }
  return buffers_[index];
}

std::uint8_t *LogBuffers::logMetaDataBuffer() const {
  return buffer(LogBufferDescriptor::LOG_META_DATA_SECTION_INDEX);
}

void LogBuffers::sync() {
  if (memoryMappedFile_) {
    memoryMappedFile_->sync();
  }
}

void LogBuffers::initializeBuffers() {
  // Get metadata from the log metadata buffer
  std::uint8_t *logMetaDataBuffer =
      basePtr_ + (logLength_ - LogBufferDescriptor::LOG_META_DATA_LENGTH);

  // Read metadata values
  termLength_ = LogBufferDescriptor::termLength(logMetaDataBuffer);
  pageSize_ = LogBufferDescriptor::pageSize(logMetaDataBuffer);
  initialTermId_ = LogBufferDescriptor::initialTermId(logMetaDataBuffer);
  mtuLength_ = LogBufferDescriptor::mtuLength(logMetaDataBuffer);
  correlationId_ = LogBufferDescriptor::correlationId(logMetaDataBuffer);

  // Validate term length and page size
  LogBufferDescriptor::checkTermLength(termLength_);
  LogBufferDescriptor::checkPageSize(pageSize_);

  // Initialize buffer pointers
  buffers_.resize(LogBufferDescriptor::PARTITION_COUNT + 1);

  // Set up term buffers
  for (int i = 0; i < LogBufferDescriptor::PARTITION_COUNT; i++) {
    buffers_[i] = basePtr_ + (i * termLength_);
  }

  // Set up metadata buffer
  buffers_[LogBufferDescriptor::LOG_META_DATA_SECTION_INDEX] =
      logMetaDataBuffer;
}

void LogBuffers::preTouchPages() {
  // Pre-touch each page in the term buffers to avoid page faults
  for (int i = 0; i < LogBufferDescriptor::PARTITION_COUNT; i++) {
    std::uint8_t *termBuffer = buffers_[i];

    for (std::int32_t offset = 0; offset < termLength_; offset += pageSize_) {
      // Touch each page by reading a byte
      volatile std::uint8_t value = termBuffer[offset];
      (void)value; // Suppress unused variable warning
    }
  }
}

void LogBuffers::initializeNewLogFile(std::int32_t termLength) {
  // Get the metadata buffer
  std::uint8_t *logMetaDataBuffer =
      basePtr_ + (logLength_ - LogBufferDescriptor::LOG_META_DATA_LENGTH);

  // Initialize metadata with default values
  const std::int32_t pageSize = 4096; // 4KB page size
  const std::int32_t initialTermId = 1;
  const std::int32_t mtuLength = 1408; // Standard MTU
  const std::int64_t correlationId = 1;

  // Zero out the metadata buffer first
  std::memset(logMetaDataBuffer, 0, LogBufferDescriptor::LOG_META_DATA_LENGTH);

  // Set the metadata values
  LogBufferDescriptor::setTermLength(logMetaDataBuffer, termLength);
  LogBufferDescriptor::setPageSize(logMetaDataBuffer, pageSize);
  LogBufferDescriptor::setInitialTermId(logMetaDataBuffer, initialTermId);
  LogBufferDescriptor::setMtuLength(logMetaDataBuffer, mtuLength);
  LogBufferDescriptor::setCorrelationId(logMetaDataBuffer, correlationId);

  // Initialize active term count to 0
  LogBufferDescriptor::activeTermCount(logMetaDataBuffer, 0);

  // Initialize tail counters to 0 for all partitions
  for (int i = 0; i < LogBufferDescriptor::PARTITION_COUNT; i++) {
    LogBufferDescriptor::tailCounter(logMetaDataBuffer, i, 0);
  }
}

} // namespace logbuffer
} // namespace aeron
