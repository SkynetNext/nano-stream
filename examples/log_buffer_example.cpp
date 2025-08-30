#include "aeron/logbuffer/frame_descriptor.h"
#include "aeron/logbuffer/log_buffers.h"
#include "aeron/util/path_utils.h"
#include <cstring>
#include <iostream>
#include <memory>

int main() {
  std::cout << "=== Aeron Log Buffer Example ===" << std::endl;

  try {
    // Create a log buffer file
    std::string logFileName = aeron::util::PathUtils::join_path(
        aeron::util::PathUtils::get_temp_dir(), "test-log-buffer");

    // Create a simple log buffer with 3 terms of 64KB each
    const std::int32_t termLength = 64 * 1024;
    const std::int64_t logLength =
        (termLength * 3) +
        aeron::logbuffer::LogBufferDescriptor::LOG_META_DATA_LENGTH;

    // Create memory mapped file
    auto memoryMappedFile = std::make_unique<aeron::util::MemoryMappedFile>(
        logFileName, logLength, true);

    if (!memoryMappedFile->is_valid()) {
      std::cerr << "Failed to create log buffer file" << std::endl;
      return 1;
    }

    // Initialize log metadata
    std::uint8_t *logMetaDataBuffer =
        memoryMappedFile->memory() +
        (logLength -
         aeron::logbuffer::LogBufferDescriptor::LOG_META_DATA_LENGTH);

    // Set metadata values
    aeron::logbuffer::LogBufferDescriptor::setTermLength(logMetaDataBuffer,
                                                         termLength);
    aeron::logbuffer::LogBufferDescriptor::setPageSize(logMetaDataBuffer, 4096);
    aeron::logbuffer::LogBufferDescriptor::setInitialTermId(logMetaDataBuffer,
                                                            0);
    aeron::logbuffer::LogBufferDescriptor::setMtuLength(logMetaDataBuffer,
                                                        1408);
    aeron::logbuffer::LogBufferDescriptor::setCorrelationId(logMetaDataBuffer,
                                                            12345);
    aeron::logbuffer::LogBufferDescriptor::activeTermCount(logMetaDataBuffer,
                                                           0);

    // Initialize tail counters
    for (int i = 0; i < aeron::logbuffer::LogBufferDescriptor::PARTITION_COUNT;
         i++) {
      aeron::logbuffer::LogBufferDescriptor::tailCounter(logMetaDataBuffer, i,
                                                         0);
    }

    // Create LogBuffers object
    aeron::logbuffer::LogBuffers logBuffers(logFileName);

    std::cout << "Log buffer created successfully:" << std::endl;
    std::cout << "  Term length: " << logBuffers.termLength() << std::endl;
    std::cout << "  Page size: " << logBuffers.pageSize() << std::endl;
    std::cout << "  Initial term ID: " << logBuffers.initialTermId()
              << std::endl;
    std::cout << "  MTU length: " << logBuffers.mtuLength() << std::endl;
    std::cout << "  Correlation ID: " << logBuffers.correlationId()
              << std::endl;

    // Write a test message to term 0
    std::uint8_t *termBuffer = logBuffers.buffer(0);
    std::int32_t termOffset = 0;

    // Create a test message
    std::string testMessage = "Hello, Aeron Log Buffer!";
    std::int32_t messageLength =
        static_cast<std::int32_t>(testMessage.length());
    std::int32_t frameLength =
        messageLength + aeron::logbuffer::FrameDescriptor::HEADER_LENGTH;

    // Write frame header
    aeron::logbuffer::FrameDescriptor::frameLength(termBuffer, termOffset,
                                                   frameLength);
    aeron::logbuffer::FrameDescriptor::frameVersion(termBuffer, termOffset, 1);
    aeron::logbuffer::FrameDescriptor::frameFlags(
        termBuffer, termOffset,
        aeron::logbuffer::FrameDescriptor::UNFRAGMENTED);
    aeron::logbuffer::FrameDescriptor::frameType(
        termBuffer, termOffset,
        aeron::logbuffer::FrameDescriptor::HDR_TYPE_DATA);
    aeron::logbuffer::FrameDescriptor::termOffset(termBuffer, termOffset,
                                                  termOffset);
    aeron::logbuffer::FrameDescriptor::sessionId(termBuffer, termOffset, 1);
    aeron::logbuffer::FrameDescriptor::streamId(termBuffer, termOffset, 100);
    aeron::logbuffer::FrameDescriptor::termId(termBuffer, termOffset, 0);
    aeron::logbuffer::FrameDescriptor::reservedValue(termBuffer, termOffset, 0);

    // Write message data
    std::memcpy(termBuffer + termOffset +
                    aeron::logbuffer::FrameDescriptor::HEADER_LENGTH,
                testMessage.c_str(), messageLength);

    // Update tail counter
    aeron::logbuffer::LogBufferDescriptor::tailCounter(
        logBuffers.logMetaDataBuffer(), 0, frameLength);

    std::cout << "Wrote message to term 0: \"" << testMessage << "\""
              << std::endl;
    std::cout << "Frame length: " << frameLength << std::endl;
    std::cout << "Tail counter: "
              << aeron::logbuffer::LogBufferDescriptor::tailCounter(
                     logBuffers.logMetaDataBuffer(), 0)
              << std::endl;

    // Read the message back
    std::uint8_t *readBuffer = logBuffers.buffer(0);
    std::int32_t readOffset = 0;

    // Read frame header
    std::int32_t readFrameLength =
        aeron::logbuffer::FrameDescriptor::frameLength(readBuffer, readOffset);
    std::uint8_t readFrameType =
        aeron::logbuffer::FrameDescriptor::frameType(readBuffer, readOffset);
    std::uint8_t readFrameFlags =
        aeron::logbuffer::FrameDescriptor::frameFlags(readBuffer, readOffset);

    std::cout << "Read frame:" << std::endl;
    std::cout << "  Frame length: " << readFrameLength << std::endl;
    std::cout << "  Frame type: " << static_cast<int>(readFrameType)
              << std::endl;
    std::cout << "  Frame flags: " << static_cast<int>(readFrameFlags)
              << std::endl;

    if (readFrameType == aeron::logbuffer::FrameDescriptor::HDR_TYPE_DATA) {
      // Read message data
      std::string readMessage(
          reinterpret_cast<const char *>(
              readBuffer + readOffset +
              aeron::logbuffer::FrameDescriptor::HEADER_LENGTH),
          readFrameLength - aeron::logbuffer::FrameDescriptor::HEADER_LENGTH);

      std::cout << "  Message: \"" << readMessage << "\"" << std::endl;
    }

    // Sync to disk
    logBuffers.sync();
    std::cout << "Log buffer synced to disk" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  std::cout << "Log buffer example completed successfully" << std::endl;
  return 0;
}
