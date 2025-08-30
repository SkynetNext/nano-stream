#pragma once

#include <cstdint>

namespace aeron {
namespace logbuffer {

/**
 * Layout description for log buffers which contains partitions of terms with
 * associated term metadata, plus ending with overall log metadata.
 *
 * +----------------------------+
 * |           Term 0           |
 * +----------------------------+
 * |           Term 1           |
 * +----------------------------+
 * |           Term 2           |
 * +----------------------------+
 * |        Log Meta Data       |
 * +----------------------------+
 */
class LogBufferDescriptor {
public:
  static constexpr int PADDING_SIZE = 64;

  /**
   * The number of partitions the log is divided into.
   */
  static constexpr int PARTITION_COUNT = 3;

  /**
   * Section index for which buffer contains the log metadata.
   */
  static constexpr int LOG_META_DATA_SECTION_INDEX = PARTITION_COUNT;

  /**
   * Minimum buffer length for a log term.
   */
  static constexpr int TERM_MIN_LENGTH = 64 * 1024;

  /**
   * Maximum buffer length for a log term.
   */
  static constexpr int TERM_MAX_LENGTH = 1024 * 1024 * 1024;

  /**
   * Minimum page size.
   */
  static constexpr int PAGE_MIN_SIZE = 4 * 1024;

  /**
   * Maximum page size.
   */
  static constexpr int PAGE_MAX_SIZE = 1024 * 1024 * 1024;

  // *******************************
  // *** Log Meta Data Constants ***
  // *******************************

  /**
   * Offset within the metadata where the tail values are stored.
   */
  static constexpr int TERM_TAIL_COUNTERS_OFFSET = 0;

  /**
   * Offset within the log metadata where the active partition index is stored.
   */
  static constexpr int LOG_ACTIVE_TERM_COUNT_OFFSET =
      TERM_TAIL_COUNTERS_OFFSET + (PARTITION_COUNT * sizeof(std::int64_t));

  /**
   * Offset within the log metadata where the position of the End of Stream is
   * stored.
   */
  static constexpr int LOG_END_OF_STREAM_POSITION_OFFSET =
      LOG_ACTIVE_TERM_COUNT_OFFSET + sizeof(std::int32_t) + PADDING_SIZE;

  /**
   * Offset within the log metadata where whether the log is connected or not is
   * stored.
   */
  static constexpr int LOG_IS_CONNECTED_OFFSET =
      LOG_END_OF_STREAM_POSITION_OFFSET + sizeof(std::int64_t);

  /**
   * Offset within the log metadata where the count of active transports is
   * stored.
   */
  static constexpr int LOG_ACTIVE_TRANSPORT_COUNT =
      LOG_IS_CONNECTED_OFFSET + sizeof(std::int32_t);

  /**
   * Offset within the log metadata where the active term id is stored.
   */
  static constexpr int LOG_INITIAL_TERM_ID_OFFSET =
      LOG_ACTIVE_TRANSPORT_COUNT + sizeof(std::int32_t) + PADDING_SIZE;

  /**
   * Offset within the log metadata which the length field for the frame header
   * is stored.
   */
  static constexpr int LOG_DEFAULT_FRAME_HEADER_LENGTH_OFFSET =
      LOG_INITIAL_TERM_ID_OFFSET + sizeof(std::int32_t);

  /**
   * Offset within the log metadata which the MTU length is stored.
   */
  static constexpr int LOG_MTU_LENGTH_OFFSET =
      LOG_DEFAULT_FRAME_HEADER_LENGTH_OFFSET + sizeof(std::int32_t);

  /**
   * Offset within the log metadata which the correlation id is stored.
   */
  static constexpr int LOG_CORRELATION_ID_OFFSET =
      LOG_MTU_LENGTH_OFFSET + sizeof(std::int32_t);

  /**
   * Offset within the log metadata which the term length is stored.
   */
  static constexpr int LOG_TERM_LENGTH_OFFSET =
      LOG_CORRELATION_ID_OFFSET + sizeof(std::int64_t);

  /**
   * Offset within the log metadata which the page size is stored.
   */
  static constexpr int LOG_PAGE_SIZE_OFFSET =
      LOG_TERM_LENGTH_OFFSET + sizeof(std::int32_t);

  /**
   * Offset at which the default frame headers begin.
   */
  static constexpr int LOG_DEFAULT_FRAME_HEADER_OFFSET =
      LOG_PAGE_SIZE_OFFSET + sizeof(std::int32_t) + PADDING_SIZE;

  /**
   * Maximum length of a frame header.
   */
  static constexpr int LOG_DEFAULT_FRAME_HEADER_MAX_LENGTH = PADDING_SIZE * 2;

  /**
   * Total length of the log metadata buffer in bytes.
   */
  static constexpr int LOG_META_DATA_LENGTH =
      LOG_DEFAULT_FRAME_HEADER_OFFSET + LOG_DEFAULT_FRAME_HEADER_MAX_LENGTH;

  /**
   * Check if the term length is valid.
   */
  static void checkTermLength(int termLength);

  /**
   * Check if the page size is valid.
   */
  static void checkPageSize(int pageSize);

  /**
   * Get the term length from the log metadata.
   */
  static int termLength(const std::uint8_t *logMetaDataBuffer);

  /**
   * Get the page size from the log metadata.
   */
  static int pageSize(const std::uint8_t *logMetaDataBuffer);

  /**
   * Get the initial term id from the log metadata.
   */
  static int initialTermId(const std::uint8_t *logMetaDataBuffer);

  /**
   * Get the MTU length from the log metadata.
   */
  static int mtuLength(const std::uint8_t *logMetaDataBuffer);

  /**
   * Get the correlation id from the log metadata.
   */
  static std::int64_t correlationId(const std::uint8_t *logMetaDataBuffer);

  /**
   * Get the active term count from the log metadata.
   */
  static int activeTermCount(const std::uint8_t *logMetaDataBuffer);

  /**
   * Set the active term count in the log metadata.
   */
  static void activeTermCount(std::uint8_t *logMetaDataBuffer, int termCount);

  /**
   * Set the term length in the log metadata.
   */
  static void setTermLength(std::uint8_t *logMetaDataBuffer, int termLength);

  /**
   * Set the page size in the log metadata.
   */
  static void setPageSize(std::uint8_t *logMetaDataBuffer, int pageSize);

  /**
   * Set the initial term id in the log metadata.
   */
  static void setInitialTermId(std::uint8_t *logMetaDataBuffer,
                               int initialTermId);

  /**
   * Set the MTU length in the log metadata.
   */
  static void setMtuLength(std::uint8_t *logMetaDataBuffer, int mtuLength);

  /**
   * Set the correlation id in the log metadata.
   */
  static void setCorrelationId(std::uint8_t *logMetaDataBuffer,
                               std::int64_t correlationId);

  /**
   * Get the tail counter offset for a given partition.
   */
  static int tailCounterOffset(int partitionIndex);

  /**
   * Get the tail counter value for a given partition.
   */
  static std::int64_t tailCounter(const std::uint8_t *logMetaDataBuffer,
                                  int partitionIndex);

  /**
   * Set the tail counter value for a given partition.
   */
  static void tailCounter(std::uint8_t *logMetaDataBuffer, int partitionIndex,
                          std::int64_t value);

  /**
   * Compute the position from term id and term offset.
   */
  static std::int64_t computePosition(int termId, int termOffset,
                                      int positionBitsToShift,
                                      int initialTermId);

  /**
   * Compute the term id from position.
   */
  static int computeTermId(std::int64_t position, int positionBitsToShift,
                           int initialTermId);

  /**
   * Compute the term offset from position.
   */
  static int computeTermOffset(std::int64_t position, int termLength);

  /**
   * Get the partition index from term count.
   */
  static int indexByTermCount(int termCount);

  /**
   * Compute the term count from term id and initial term id.
   */
  static int computeTermCount(int termId, int initialTermId);

  /**
   * Check if the position is within the term.
   */
  static bool isWithinTerm(std::int64_t position, int termId, int termLength,
                           int positionBitsToShift, int initialTermId);

  /**
   * Get the position bits to shift for a given term length.
   */
  static int positionBitsToShift(int termLength);

  /**
   * Get the partition index from position.
   */
  static int indexByPosition(std::int64_t position, int positionBitsToShift);

  /**
   * Compute the term id from position.
   */
  static int computeTermIdFromPosition(std::int64_t position,
                                       int positionBitsToShift,
                                       int initialTermId);
};

} // namespace logbuffer
} // namespace aeron
