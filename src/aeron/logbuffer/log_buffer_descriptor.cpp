#include "aeron/logbuffer/log_buffer_descriptor.h"
#include <stdexcept>
#include <string>

namespace aeron {
namespace logbuffer {

void LogBufferDescriptor::checkTermLength(int termLength) {
  if (termLength < TERM_MIN_LENGTH) {
    throw std::invalid_argument("Term length must be >= " +
                                std::to_string(TERM_MIN_LENGTH));
  }
  if (termLength > TERM_MAX_LENGTH) {
    throw std::invalid_argument("Term length must be <= " +
                                std::to_string(TERM_MAX_LENGTH));
  }
  if ((termLength & (termLength - 1)) != 0) {
    throw std::invalid_argument("Term length must be a power of 2");
  }
}

void LogBufferDescriptor::checkPageSize(int pageSize) {
  if (pageSize < PAGE_MIN_SIZE) {
    throw std::invalid_argument("Page size must be >= " +
                                std::to_string(PAGE_MIN_SIZE));
  }
  if (pageSize > PAGE_MAX_SIZE) {
    throw std::invalid_argument("Page size must be <= " +
                                std::to_string(PAGE_MAX_SIZE));
  }
  if ((pageSize & (pageSize - 1)) != 0) {
    throw std::invalid_argument("Page size must be a power of 2");
  }
}

int LogBufferDescriptor::termLength(const std::uint8_t *logMetaDataBuffer) {
  return *reinterpret_cast<const std::int32_t *>(logMetaDataBuffer +
                                                 LOG_TERM_LENGTH_OFFSET);
}

int LogBufferDescriptor::pageSize(const std::uint8_t *logMetaDataBuffer) {
  return *reinterpret_cast<const std::int32_t *>(logMetaDataBuffer +
                                                 LOG_PAGE_SIZE_OFFSET);
}

int LogBufferDescriptor::initialTermId(const std::uint8_t *logMetaDataBuffer) {
  return *reinterpret_cast<const std::int32_t *>(logMetaDataBuffer +
                                                 LOG_INITIAL_TERM_ID_OFFSET);
}

int LogBufferDescriptor::mtuLength(const std::uint8_t *logMetaDataBuffer) {
  return *reinterpret_cast<const std::int32_t *>(logMetaDataBuffer +
                                                 LOG_MTU_LENGTH_OFFSET);
}

std::int64_t
LogBufferDescriptor::correlationId(const std::uint8_t *logMetaDataBuffer) {
  return *reinterpret_cast<const std::int64_t *>(logMetaDataBuffer +
                                                 LOG_CORRELATION_ID_OFFSET);
}

int LogBufferDescriptor::activeTermCount(
    const std::uint8_t *logMetaDataBuffer) {
  return *reinterpret_cast<const std::int32_t *>(logMetaDataBuffer +
                                                 LOG_ACTIVE_TERM_COUNT_OFFSET);
}

void LogBufferDescriptor::activeTermCount(std::uint8_t *logMetaDataBuffer,
                                          int termCount) {
  *reinterpret_cast<std::int32_t *>(logMetaDataBuffer +
                                    LOG_ACTIVE_TERM_COUNT_OFFSET) = termCount;
}

void LogBufferDescriptor::setTermLength(std::uint8_t *logMetaDataBuffer,
                                        int termLength) {
  *reinterpret_cast<std::int32_t *>(logMetaDataBuffer +
                                    LOG_TERM_LENGTH_OFFSET) = termLength;
}

void LogBufferDescriptor::setPageSize(std::uint8_t *logMetaDataBuffer,
                                      int pageSize) {
  *reinterpret_cast<std::int32_t *>(logMetaDataBuffer + LOG_PAGE_SIZE_OFFSET) =
      pageSize;
}

void LogBufferDescriptor::setInitialTermId(std::uint8_t *logMetaDataBuffer,
                                           int initialTermId) {
  *reinterpret_cast<std::int32_t *>(logMetaDataBuffer +
                                    LOG_INITIAL_TERM_ID_OFFSET) = initialTermId;
}

void LogBufferDescriptor::setMtuLength(std::uint8_t *logMetaDataBuffer,
                                       int mtuLength) {
  *reinterpret_cast<std::int32_t *>(logMetaDataBuffer + LOG_MTU_LENGTH_OFFSET) =
      mtuLength;
}

void LogBufferDescriptor::setCorrelationId(std::uint8_t *logMetaDataBuffer,
                                           std::int64_t correlationId) {
  *reinterpret_cast<std::int64_t *>(logMetaDataBuffer +
                                    LOG_CORRELATION_ID_OFFSET) = correlationId;
}

int LogBufferDescriptor::tailCounterOffset(int partitionIndex) {
  return TERM_TAIL_COUNTERS_OFFSET + (partitionIndex * sizeof(std::int64_t));
}

std::int64_t
LogBufferDescriptor::tailCounter(const std::uint8_t *logMetaDataBuffer,
                                 int partitionIndex) {
  return *reinterpret_cast<const std::int64_t *>(
      logMetaDataBuffer + tailCounterOffset(partitionIndex));
}

void LogBufferDescriptor::tailCounter(std::uint8_t *logMetaDataBuffer,
                                      int partitionIndex, std::int64_t value) {
  *reinterpret_cast<std::int64_t *>(logMetaDataBuffer +
                                    tailCounterOffset(partitionIndex)) = value;
}

std::int64_t LogBufferDescriptor::computePosition(int termId, int termOffset,
                                                  int positionBitsToShift,
                                                  int initialTermId) {
  const std::int64_t termCount = termId - initialTermId;
  return (termCount << positionBitsToShift) + termOffset;
}

int LogBufferDescriptor::computeTermId(std::int64_t position,
                                       int positionBitsToShift,
                                       int initialTermId) {
  return static_cast<int>((position >> positionBitsToShift) + initialTermId);
}

int LogBufferDescriptor::computeTermOffset(std::int64_t position,
                                           int termLength) {
  return static_cast<int>(position & (termLength - 1));
}

int LogBufferDescriptor::indexByTermCount(int termCount) {
  return termCount % PARTITION_COUNT;
}

int LogBufferDescriptor::computeTermCount(int termId, int initialTermId) {
  return termId - initialTermId;
}

bool LogBufferDescriptor::isWithinTerm(std::int64_t position, int termId,
                                       int termLength, int positionBitsToShift,
                                       int initialTermId) {
  const int computedTermId =
      computeTermId(position, positionBitsToShift, initialTermId);
  return computedTermId == termId;
}

int LogBufferDescriptor::positionBitsToShift(int termLength) {
  // Calculate the number of bits to shift based on term length
  // termLength should be a power of 2
  int bits = 0;
  int temp = termLength;
  while (temp > 1) {
    temp >>= 1;
    bits++;
  }
  return bits;
}

int LogBufferDescriptor::indexByPosition(std::int64_t position,
                                         int positionBitsToShift) {
  return static_cast<int>((position >> positionBitsToShift) % PARTITION_COUNT);
}

int LogBufferDescriptor::computeTermIdFromPosition(std::int64_t position,
                                                   int positionBitsToShift,
                                                   int initialTermId) {
  return static_cast<int>((position >> positionBitsToShift) + initialTermId);
}

} // namespace logbuffer
} // namespace aeron
