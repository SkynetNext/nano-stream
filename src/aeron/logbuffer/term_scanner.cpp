#include "aeron/logbuffer/term_scanner.h"
#include "aeron/logbuffer/frame_descriptor.h"
#include <algorithm>

namespace aeron {
namespace logbuffer {

std::int64_t TermScanner::scanForAvailability(const std::uint8_t *termBuffer,
                                              std::int32_t offset,
                                              std::int32_t maxLength,
                                              std::int32_t termLength) {
  const std::int32_t limit = std::min(maxLength, termLength - offset);
  std::int32_t available = 0;
  std::int32_t padding = 0;

  do {
    const std::int32_t termOffset = offset + available;
    const std::int32_t frameLength =
        FrameDescriptor::frameLength(termBuffer, termOffset);

    if (frameLength <= 0) {
      break;
    }

    std::int32_t alignedFrameLength =
        FrameDescriptor::align(frameLength, FrameDescriptor::FRAME_ALIGNMENT);

    if (FrameDescriptor::frameType(termBuffer, termOffset) ==
        FrameDescriptor::HDR_TYPE_PAD) {
      padding = alignedFrameLength - FrameDescriptor::HEADER_LENGTH;
      alignedFrameLength = FrameDescriptor::HEADER_LENGTH;
    }

    available += alignedFrameLength;

    if (available > limit) {
      available = alignedFrameLength == available
                      ? -available
                      : available - alignedFrameLength;
      padding = 0;
      break;
    }
  } while (0 == padding && available < limit);

  return pack(padding, available);
}

} // namespace logbuffer
} // namespace aeron
