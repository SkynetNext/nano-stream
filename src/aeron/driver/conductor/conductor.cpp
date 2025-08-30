#include "aeron/driver/conductor/conductor.h"
#include <chrono>
#include <iostream>

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
  // Placeholder implementation - would read from shared memory buffer
  // and process control messages from clients

  // In a real implementation, this would:
  // 1. Read messages from control_request_buffer_
  // 2. Parse the message type and dispatch to appropriate handler
  // 3. Send responses via control_response_buffer_
}

void Conductor::handle_add_publication(
    const protocol::PublicationMessage &message) {
  std::lock_guard<std::mutex> lock(publications_mutex_);

  // Generate unique session and registration IDs
  std::int32_t session_id = generate_session_id();
  std::int64_t registration_id = generate_registration_id();

  // Create publication record
  auto publication = std::make_unique<Publication>();
  publication->session_id = session_id;
  publication->stream_id = message.stream_id;
  publication->channel =
      std::string(message.channel_data(), message.channel_length);
  publication->registration_id = registration_id;
  publication->client_id = message.header.client_id;

  publications_[registration_id] = std::move(publication);

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
  subscription->channel =
      std::string(message.channel_data(), message.channel_length);
  subscription->registration_id = registration_id;
  subscription->client_id = message.header.client_id;

  subscriptions_[registration_id] = std::move(subscription);

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

void Conductor::send_response(const protocol::ResponseMessage &response) {
  // Placeholder implementation - would write to control_response_buffer_
  // In a real implementation, this would serialize the response and write it
  // to the shared memory buffer for the client to read
}

std::int32_t Conductor::generate_session_id() {
  return next_session_id_.fetch_add(1);
}

std::int64_t Conductor::generate_registration_id() {
  return next_registration_id_.fetch_add(1);
}

} // namespace driver
} // namespace aeron
