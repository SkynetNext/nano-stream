#include "aeron/driver/conductor/conductor.h"
#include "aeron/driver/media_driver.h"
#include <chrono>
#include <iostream>

// Forward declaration
namespace aeron {
namespace driver {
// We'll use a different approach to get the aeron directory
extern MediaDriver *g_media_driver;
} // namespace driver
} // namespace aeron

namespace aeron {
namespace driver {

Conductor::Conductor() {
  // Control buffers will be initialized by Media Driver
  // and passed to Conductor later
}

Conductor::~Conductor() { stop(); }

void Conductor::set_control_buffers(
    std::unique_ptr<util::MemoryMappedFile> request_buffer,
    std::unique_ptr<util::MemoryMappedFile> response_buffer) {
  control_request_buffer_ = std::move(request_buffer);
  control_response_buffer_ = std::move(response_buffer);
}

void Conductor::set_log_buffer_manager(
    std::shared_ptr<LogBufferManager> manager) {
  log_buffer_manager_ = manager;
}

void Conductor::set_aeron_directory(const std::string &aeron_dir) {
  aeron_dir_ = aeron_dir;
}

void Conductor::set_sender(std::unique_ptr<class Sender> &sender) {
  sender_ = &sender;
}

void Conductor::set_receiver(std::unique_ptr<class Receiver> &receiver) {
  receiver_ = &receiver;
}

void Conductor::start() {
  if (running_.load()) {
    return;
  }

  running_.store(true);
  conductor_thread_ = std::thread(&Conductor::run, this);
  std::cout << "Conductor started" << std::endl;
}

void Conductor::stop() {
  if (!running_.load()) {
    return;
  }

  running_.store(false);
  if (conductor_thread_.joinable()) {
    conductor_thread_.join();
  }
  std::cout << "Conductor stopped" << std::endl;
}

void Conductor::run() {
  auto last_timeout_check = std::chrono::steady_clock::now();

  while (running_.load()) {
    try {
      // Process control messages from clients
      process_control_messages();

      // Check for client timeouts periodically
      auto now = std::chrono::steady_clock::now();
      if (std::chrono::duration_cast<std::chrono::milliseconds>(
              now - last_timeout_check)
              .count() >= 1000) {
        check_client_timeouts();
        last_timeout_check = now;
      }

      // Sleep for a short duration to avoid busy waiting
      std::this_thread::sleep_for(
          std::chrono::milliseconds(CONDUCTOR_TICK_DURATION_MS));

    } catch (const std::exception &e) {
      std::cerr << "Conductor error: " << e.what() << std::endl;
    }
  }
}

void Conductor::process_control_messages() {
  if (!control_request_buffer_ || !control_response_buffer_) {
    return;
  }

  // Read control messages from shared memory buffer
  // This is a simplified implementation - in the real Aeron, this would use
  // a more sophisticated buffer management system

  try {
    // Read message header to determine message type
    protocol::ControlMessageHeader header;
    if (read_control_message_header(header)) {
      switch (header.type) {
      case protocol::ControlMessageType::ADD_PUBLICATION:
        handle_add_publication_message();
        break;
      case protocol::ControlMessageType::REMOVE_PUBLICATION:
        handle_remove_publication_message();
        break;
      case protocol::ControlMessageType::ADD_SUBSCRIPTION:
        handle_add_subscription_message();
        break;
      case protocol::ControlMessageType::REMOVE_SUBSCRIPTION:
        handle_remove_subscription_message();
        break;
      case protocol::ControlMessageType::CLIENT_KEEPALIVE:
        handle_client_keepalive_message();
        break;
      default:
        std::cerr << "Unknown message type: " << static_cast<int>(header.type)
                  << std::endl;
        break;
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "Error processing control messages: " << e.what() << std::endl;
  }
}

void Conductor::handle_add_publication(
    const protocol::PublicationMessage &message) {
  std::lock_guard<std::mutex> lock(publications_mutex_);

  // Generate unique session ID, but use correlation_id as registration_id
  // to match what the client expects
  std::int32_t session_id = generate_session_id();
  std::int64_t registration_id = message.header.correlation_id;

  // Create publication record
  auto publication = std::make_unique<Publication>();
  publication->session_id = session_id;
  publication->stream_id = message.stream_id;
  publication->channel =
      std::string(message.channel_data(), message.channel_length);
  publication->registration_id = registration_id;
  publication->client_id = message.header.client_id;

  publications_[registration_id] = std::move(publication);

  // Add to log buffer manager for Sender to process
  if (log_buffer_manager_) {
    // Create log buffers for this publication - use the same file name as
    // client
    std::string log_file_name = "pub-" + std::to_string(registration_id);
    std::string full_log_path =
        util::PathUtils::join_path(aeron_dir_, log_file_name);

    // Create log buffers with the full path
    auto log_buffers =
        std::make_shared<logbuffer::LogBuffers>(full_log_path, false);
    log_buffer_manager_->add_publication(session_id, message.stream_id,
                                         registration_id, log_buffers);

    std::cout << "Added publication to log buffer manager: session="
              << session_id << ", stream=" << message.stream_id
              << ", file=" << full_log_path << std::endl;
  }

  // Also add to Sender for network transmission
  if (sender_) {
    std::string channel =
        std::string(message.channel_data(), message.channel_length);
    (*sender_)->add_publication(session_id, message.stream_id, channel);
  }

  // Send success response
  protocol::ResponseMessage response;
  response.init_success(message.header.correlation_id, registration_id);
  send_response(response);

  std::cout << "Added publication: stream=" << message.stream_id
            << ", session=" << session_id
            << ", registration=" << registration_id << std::endl;
}

void Conductor::handle_remove_publication(std::int64_t registration_id,
                                          std::int64_t client_id) {
  std::lock_guard<std::mutex> lock(publications_mutex_);

  auto it = publications_.find(registration_id);
  if (it != publications_.end() && it->second->client_id == client_id) {
    publications_.erase(it);
    std::cout << "Removed publication: registration=" << registration_id
              << std::endl;
  }
}

void Conductor::handle_add_subscription(
    const protocol::SubscriptionMessage &message) {
  std::lock_guard<std::mutex> lock(subscriptions_mutex_);

  // Generate unique registration ID
  std::int64_t registration_id = generate_registration_id();

  // Create subscription record
  auto subscription = std::make_unique<Subscription>();
  subscription->stream_id = message.stream_id;

  // Debug: Check channel_length issue
  std::cout << "=== DEBUG: channel_length = " << message.channel_length
            << " (should be ~50-100) ===" << std::endl;
  std::cout.flush();

  // Check if the length is reasonable
  if (message.channel_length <= 0 || message.channel_length > 1000) {
    std::cout << "=== ERROR: Invalid channel_length: " << message.channel_length
              << " ===" << std::endl;
    std::cout.flush();
    return; // Exit early to avoid crash
  }

  subscription->channel =
      std::string(message.channel_data(), message.channel_length);

  subscription->registration_id = registration_id;
  subscription->client_id = message.header.client_id;

  subscriptions_[registration_id] = std::move(subscription);

  // Also add to Receiver for network reception
  if (receiver_) {
    std::string channel =
        std::string(message.channel_data(), message.channel_length);

    (*receiver_)->add_subscription(message.stream_id, channel);
  } else {
    std::cout << "=== WARNING: Receiver is null ===" << std::endl;
  }

  // Send success response
  protocol::ResponseMessage response;
  response.init_success(message.header.correlation_id, registration_id);
  send_response(response);

  std::cout << "Added subscription: stream=" << message.stream_id
            << ", registration=" << registration_id << std::endl;
}

void Conductor::handle_remove_subscription(std::int64_t registration_id,
                                           std::int64_t client_id) {
  std::lock_guard<std::mutex> lock(subscriptions_mutex_);

  auto it = subscriptions_.find(registration_id);
  if (it != subscriptions_.end() && it->second->client_id == client_id) {
    subscriptions_.erase(it);
    std::cout << "Removed subscription: registration=" << registration_id
              << std::endl;
  }
}

void Conductor::handle_client_keepalive(std::int64_t client_id) {
  std::lock_guard<std::mutex> lock(clients_mutex_);

  auto it = clients_.find(client_id);
  if (it != clients_.end()) {
    it->second->last_keepalive_time =
        std::chrono::steady_clock::now().time_since_epoch().count();
  } else {
    // New client
    auto client = std::make_unique<ClientSession>();
    client->client_id = client_id;
    client->last_keepalive_time =
        std::chrono::steady_clock::now().time_since_epoch().count();
    clients_[client_id] = std::move(client);
    std::cout << "New client connected: " << client_id << std::endl;
  }
}

void Conductor::check_client_timeouts() {
  std::lock_guard<std::mutex> lock(clients_mutex_);

  auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  auto timeout_threshold =
      now - (CLIENT_TIMEOUT_MS * 1000000); // Convert to nanoseconds

  auto it = clients_.begin();
  while (it != clients_.end()) {
    if (it->second->last_keepalive_time < timeout_threshold) {
      std::cout << "Client timeout: " << it->first << std::endl;

      // Clean up client's publications and subscriptions
      {
        std::lock_guard<std::mutex> pub_lock(publications_mutex_);
        auto pub_it = publications_.begin();
        while (pub_it != publications_.end()) {
          if (pub_it->second->client_id == it->first) {
            pub_it = publications_.erase(pub_it);
          } else {
            ++pub_it;
          }
        }
      }

      {
        std::lock_guard<std::mutex> sub_lock(subscriptions_mutex_);
        auto sub_it = subscriptions_.begin();
        while (sub_it != subscriptions_.end()) {
          if (sub_it->second->client_id == it->first) {
            sub_it = subscriptions_.erase(sub_it);
          } else {
            ++sub_it;
          }
        }
      }

      it = clients_.erase(it);
    } else {
      ++it;
    }
  }
}

bool Conductor::read_control_message_header(
    protocol::ControlMessageHeader &header) {
  if (!control_request_buffer_) {
    return false;
  }

  // Read header from shared memory buffer
  // This is a simplified implementation - in the real Aeron, this would use
  // proper buffer management with read/write cursors

  std::uint8_t *buffer = control_request_buffer_->memory();
  if (!buffer) {
    return false;
  }

  // Check if there's a valid message at the beginning of the buffer
  // In a real implementation, we would check message length and validity
  std::memcpy(&header, buffer, sizeof(header));

  // Basic validation - check if the message type is valid
  if (header.type < protocol::ControlMessageType::ADD_PUBLICATION ||
      header.type > protocol::ControlMessageType::CLIENT_TIMEOUT) {
    return false; // Invalid message type
  }

  // Check if the message length is reasonable
  if (static_cast<std::size_t>(header.length) <
          sizeof(protocol::ControlMessageHeader) ||
      static_cast<std::size_t>(header.length) >
          control_request_buffer_->size()) {
    return false; // Invalid message length
  }

  return true; // Valid message found
}

void Conductor::handle_add_publication_message() {
  // Read the full publication message from shared memory
  if (!control_request_buffer_) {
    return;
  }

  std::uint8_t *buffer = control_request_buffer_->memory();
  if (!buffer) {
    return;
  }

  // Read the publication message
  protocol::PublicationMessage *message =
      reinterpret_cast<protocol::PublicationMessage *>(buffer);

  // Call the existing handler
  handle_add_publication(*message);
}

void Conductor::handle_remove_publication_message() {
  // Read the remove publication message from shared memory
  if (!control_request_buffer_) {
    return;
  }

  std::uint8_t *buffer = control_request_buffer_->memory();
  if (!buffer) {
    return;
  }

  // Read the message header to get correlation_id and client_id
  protocol::ControlMessageHeader *header =
      reinterpret_cast<protocol::ControlMessageHeader *>(buffer);

  // For now, use a placeholder registration_id
  // In a real implementation, we would read the actual registration_id from the
  // message
  std::int64_t registration_id = 0; // TODO: Read from message

  handle_remove_publication(registration_id, header->client_id);
}

void Conductor::handle_add_subscription_message() {
  // Read the full subscription message from shared memory
  if (!control_request_buffer_) {
    return;
  }

  std::uint8_t *buffer = control_request_buffer_->memory();
  if (!buffer) {
    return;
  }

  // Read the subscription message
  protocol::SubscriptionMessage *message =
      reinterpret_cast<protocol::SubscriptionMessage *>(buffer);

  // Call the existing handler
  handle_add_subscription(*message);
}

void Conductor::handle_remove_subscription_message() {
  // Read the remove subscription message from shared memory
  if (!control_request_buffer_) {
    return;
  }

  std::uint8_t *buffer = control_request_buffer_->memory();
  if (!buffer) {
    return;
  }

  // Read the message header to get correlation_id and client_id
  protocol::ControlMessageHeader *header =
      reinterpret_cast<protocol::ControlMessageHeader *>(buffer);

  // For now, use a placeholder registration_id
  // In a real implementation, we would read the actual registration_id from the
  // message
  std::int64_t registration_id = 0; // TODO: Read from message

  handle_remove_subscription(registration_id, header->client_id);
}

void Conductor::handle_client_keepalive_message() {
  // Read the client keepalive message from shared memory
  if (!control_request_buffer_) {
    return;
  }

  std::uint8_t *buffer = control_request_buffer_->memory();
  if (!buffer) {
    return;
  }

  // Read the message header to get client_id
  protocol::ControlMessageHeader *header =
      reinterpret_cast<protocol::ControlMessageHeader *>(buffer);

  handle_client_keepalive(header->client_id);
}

void Conductor::send_response(const protocol::ResponseMessage &response) {
  if (!control_response_buffer_) {
    return;
  }

  std::uint8_t *buffer = control_response_buffer_->memory();
  if (!buffer) {
    return;
  }

  // Write response to shared memory buffer
  // This is a simplified implementation - in the real Aeron, this would use
  // proper buffer management with read/write cursors

  std::memcpy(buffer, &response, response.length);
}

std::int32_t Conductor::generate_session_id() {
  return next_session_id_.fetch_add(1);
}

std::int64_t Conductor::generate_registration_id() {
  return next_registration_id_.fetch_add(1);
}

} // namespace driver
} // namespace aeron
