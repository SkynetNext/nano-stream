#ifdef _WIN32
#define NOMINMAX
#endif

#include "aeron/client/subscription.h"
#include <algorithm>
#include <mutex>

namespace aeron {
namespace client {

Image::Image(std::int32_t session_id, std::int64_t correlation_id,
             std::shared_ptr<util::MemoryMappedFile> log_buffer)
    : session_id_(session_id), correlation_id_(correlation_id),
      log_buffer_(log_buffer), term_length_(DEFAULT_TERM_LENGTH) {

  // Calculate position bits to shift for term buffer indexing
  position_bits_to_shift_ = 0;
  std::int32_t temp = term_length_;
  while (temp > 1) {
    temp >>= 1;
    position_bits_to_shift_++;
  }
}

Image::~Image() { close(); }

int Image::poll(fragment_handler_t handler, int fragment_limit) {
  if (closed_.load()) {
    return 0;
  }

  int fragments_read = 0;
  std::int64_t current_position = position_.load();

  // In a real implementation, this would:
  // 1. Read from the log buffer starting at current_position
  // 2. Parse message headers and extract fragments
  // 3. Call the handler for each fragment
  // 4. Update the position

  // For now, simulate reading some data
  for (int i = 0; i < fragment_limit && fragments_read < 5; ++i) {
    // Simulate a message
    std::string dummy_message =
        "Dummy message " + std::to_string(current_position);

    handler(reinterpret_cast<const std::uint8_t *>(dummy_message.c_str()), 0,
            dummy_message.length(), nullptr);

    current_position += dummy_message.length();
    fragments_read++;
  }

  position_.store(current_position);
  return fragments_read;
}

int Image::controlled_poll(controlled_fragment_handler_t handler,
                           int fragment_limit) {
  if (closed_.load()) {
    return 0;
  }

  int fragments_read = 0;
  std::int64_t current_position = position_.load();

  for (int i = 0; i < fragment_limit && fragments_read < 5; ++i) {
    // Simulate a message
    std::string dummy_message =
        "Controlled dummy message " + std::to_string(current_position);

    ControlledPollAction action =
        handler(reinterpret_cast<const std::uint8_t *>(dummy_message.c_str()),
                0, dummy_message.length(), nullptr);

    switch (action) {
    case ControlledPollAction::CONTINUE:
      current_position += dummy_message.length();
      fragments_read++;
      break;

    case ControlledPollAction::BREAK:
      position_.store(current_position);
      return fragments_read;

    case ControlledPollAction::ABORT:
      // Don't update position, don't count fragment
      break;

    case ControlledPollAction::COMMIT:
      current_position += dummy_message.length();
      fragments_read++;
      position_.store(current_position); // Commit immediately
      break;
    }
  }

  position_.store(current_position);
  return fragments_read;
}

bool Image::is_end_of_stream() const {
  // In a real implementation, this would check if the publisher has indicated
  // end of stream
  return false;
}

void Image::close() { closed_.store(true); }

Subscription::Subscription(const std::string &channel, std::int32_t stream_id,
                           std::int64_t registration_id)
    : channel_(channel), stream_id_(stream_id),
      registration_id_(registration_id) {}

Subscription::~Subscription() { close(); }

int Subscription::poll(fragment_handler_t handler, int fragment_limit) {
  if (closed_.load()) {
    return 0;
  }

  std::lock_guard<std::mutex> lock(images_mutex_);

  if (images_.empty()) {
    return 0;
  }

  int total_fragments = 0;
  int fragments_per_image =
      std::max(1, fragment_limit / static_cast<int>(images_.size()));

  // Round-robin polling across all images
  for (size_t i = 0; i < images_.size() && total_fragments < fragment_limit;
       ++i) {
    size_t image_index = (last_image_index_ + i) % images_.size();
    auto &image = images_[image_index];

    if (!image->is_closed()) {
      int remaining_limit = fragment_limit - total_fragments;
      int fragments_to_read = std::min(fragments_per_image, remaining_limit);

      int fragments_read = image->poll(handler, fragments_to_read);
      total_fragments += fragments_read;
    }
  }

  last_image_index_ = (last_image_index_ + 1) % images_.size();
  return total_fragments;
}

int Subscription::controlled_poll(controlled_fragment_handler_t handler,
                                  int fragment_limit) {
  if (closed_.load()) {
    return 0;
  }

  std::lock_guard<std::mutex> lock(images_mutex_);

  if (images_.empty()) {
    return 0;
  }

  int total_fragments = 0;
  int fragments_per_image =
      std::max(1, fragment_limit / static_cast<int>(images_.size()));

  // Round-robin polling across all images
  for (size_t i = 0; i < images_.size() && total_fragments < fragment_limit;
       ++i) {
    size_t image_index = (last_image_index_ + i) % images_.size();
    auto &image = images_[image_index];

    if (!image->is_closed()) {
      int remaining_limit = fragment_limit - total_fragments;
      int fragments_to_read = std::min(fragments_per_image, remaining_limit);

      int fragments_read = image->controlled_poll(handler, fragments_to_read);
      total_fragments += fragments_read;
    }
  }

  last_image_index_ = (last_image_index_ + 1) % images_.size();
  return total_fragments;
}

int Subscription::poll_image(fragment_handler_t handler, int fragment_limit,
                             std::int32_t image_session_id) {
  if (closed_.load()) {
    return 0;
  }

  std::lock_guard<std::mutex> lock(images_mutex_);

  for (auto &image : images_) {
    if (image->session_id() == image_session_id && !image->is_closed()) {
      return image->poll(handler, fragment_limit);
    }
  }

  return 0;
}

std::vector<std::shared_ptr<Image>> Subscription::images() const {
  std::lock_guard<std::mutex> lock(images_mutex_);
  return images_;
}

std::shared_ptr<Image>
Subscription::image_by_session_id(std::int32_t session_id) const {
  std::lock_guard<std::mutex> lock(images_mutex_);

  for (auto &image : images_) {
    if (image->session_id() == session_id) {
      return image;
    }
  }

  return nullptr;
}

bool Subscription::is_connected() const {
  if (closed_.load()) {
    return false;
  }

  std::lock_guard<std::mutex> lock(images_mutex_);

  // Connected if we have at least one active image
  for (auto &image : images_) {
    if (!image->is_closed()) {
      return true;
    }
  }

  return false;
}

void Subscription::close() {
  closed_.store(true);

  std::lock_guard<std::mutex> lock(images_mutex_);
  for (auto &image : images_) {
    image->close();
  }
  images_.clear();
}

void Subscription::add_image(std::shared_ptr<Image> image) {
  if (closed_.load()) {
    return;
  }

  std::lock_guard<std::mutex> lock(images_mutex_);
  images_.push_back(image);
}

void Subscription::remove_image(std::int64_t correlation_id) {
  std::lock_guard<std::mutex> lock(images_mutex_);

  auto it =
      std::remove_if(images_.begin(), images_.end(),
                     [correlation_id](const std::shared_ptr<Image> &image) {
                       return image->correlation_id() == correlation_id;
                     });

  if (it != images_.end()) {
    // Close the removed images
    for (auto iter = it; iter != images_.end(); ++iter) {
      (*iter)->close();
    }
    images_.erase(it, images_.end());
  }
}

} // namespace client
} // namespace aeron
