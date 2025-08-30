#include "aeron/client/aeron.h"
#include "aeron/util/path_utils.h"
#include <chrono>
#include <iostream>
#include <random>

namespace aeron {
namespace client {

std::shared_ptr<Aeron> Aeron::connect() {
  AeronContext default_context;
  return connect(default_context);
}

std::shared_ptr<Aeron> Aeron::connect(const AeronContext &context) {
  auto aeron = std::shared_ptr<Aeron>(new Aeron(context));
  aeron->init_connection();
  return aeron;
}

Aeron::Aeron(const AeronContext &context) : context_(context) {
  // Generate client ID if not provided
  if (context_.client_id == 0) {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    client_id_ = static_cast<std::int64_t>(gen());
  } else {
    client_id_ = context_.client_id;
  }

  last_keepalive_time_ = std::chrono::steady_clock::now();

  // Initialize connection to Media Driver
  init_connection();
}

Aeron::~Aeron() { close(); }

void Aeron::init_connection() {
  try {
    std::cout << "Connecting to Media Driver..." << std::endl;

    // Create communication buffers with media driver
    to_driver_buffer_ = std::make_unique<util::MemoryMappedFile>(
        util::PathUtils::join_path(context_.aeron_dir, "to-driver"),
        CONTROL_BUFFER_SIZE,
        false); // Open existing file created by Media Driver
    to_client_buffer_ = std::make_unique<util::MemoryMappedFile>(
        util::PathUtils::join_path(context_.aeron_dir, "to-client"),
        CONTROL_BUFFER_SIZE,
        false); // Open existing file created by Media Driver

    // Start conductor thread
    conductor_running_.store(true);
    conductor_thread_ = std::thread(&Aeron::conductor_run, this);

    connected_.store(true);
    running_.store(true);

    std::cout << "Aeron client connected: client_id=" << client_id_
              << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Failed to connect Aeron client: " << e.what() << std::endl;
    throw;
  }
}

std::shared_ptr<Publication> Aeron::add_publication(const std::string &channel,
                                                    std::int32_t stream_id) {
  if (!connected_.load()) {
    throw std::runtime_error("Aeron client not connected");
  }

  std::int64_t correlation_id = next_correlation_id();
  std::int64_t registration_id =
      correlation_id; // Use correlation_id as registration_id for simplicity

  // Create publication message
  protocol::PublicationMessage message;
  message.init(correlation_id, client_id_, stream_id, 0, channel.c_str());

  // Send control message to driver
  send_control_message(message);

  // Wait for driver to create the log buffer and get the file path
  // In a real implementation, this would be asynchronous with proper response
  // handling
  std::this_thread::sleep_for(
      std::chrono::milliseconds(100)); // Give driver time to create log buffer

  // Create publication object
  // In a real implementation, we would wait for driver response and get actual
  // session_id and log buffer file name
  std::int32_t session_id =
      static_cast<std::int32_t>(correlation_id & 0xFFFFFFFF);

  // Now open the log buffer file created by the driver
  std::string log_file_name = "pub-" + std::to_string(registration_id);
  std::string full_log_path =
      util::PathUtils::join_path(context_.aeron_dir, log_file_name);

  auto log_buffer = std::make_unique<util::MemoryMappedFile>(
      full_log_path, 0, false); // Open existing file created by Media Driver

  auto publication = std::make_shared<Publication>(
      channel, stream_id, session_id, registration_id, std::move(log_buffer));

  // Store publication
  {
    std::lock_guard<std::mutex> lock(publications_mutex_);
    publications_[registration_id] = publication;
  }

  std::cout << "Added publication: channel=" << channel
            << ", stream=" << stream_id << ", session=" << session_id
            << std::endl;

  return publication;
}

std::shared_ptr<Subscription>
Aeron::add_subscription(const std::string &channel, std::int32_t stream_id) {
  if (!connected_.load()) {
    throw std::runtime_error("Aeron client not connected");
  }

  std::int64_t correlation_id = next_correlation_id();
  std::int64_t registration_id =
      correlation_id; // Use correlation_id as registration_id for simplicity

  // Create subscription message
  protocol::SubscriptionMessage message;

  // Debug: Check structure layout
  std::cout << "=== DEBUG: SubscriptionMessage layout ===" << std::endl;
  std::cout << "sizeof(ControlMessageHeader) = "
            << sizeof(protocol::ControlMessageHeader) << std::endl;
  std::cout << "sizeof(std::int32_t) = " << sizeof(std::int32_t) << std::endl;
  std::cout << "sizeof(std::int64_t) = " << sizeof(std::int64_t) << std::endl;
  std::cout << "sizeof(SubscriptionMessage) = "
            << sizeof(protocol::SubscriptionMessage) << std::endl;
  std::cout << "BASE_LENGTH = " << protocol::SubscriptionMessage::BASE_LENGTH
            << std::endl;
  std::cout << "Expected channel_length offset = "
            << (sizeof(protocol::ControlMessageHeader) + sizeof(std::int32_t) +
                sizeof(std::int64_t))
            << std::endl;
  std::cout << "Actual channel_length offset = "
            << (reinterpret_cast<std::uint8_t *>(&message.channel_length) -
                reinterpret_cast<std::uint8_t *>(&message))
            << std::endl;
  std::cout.flush();

  // Debug: Check message state before init
  std::cout << "=== DEBUG: Before init - channel_length = "
            << message.channel_length << " ===" << std::endl;
  std::cout.flush();

  message.init(correlation_id, client_id_, stream_id, registration_id,
               channel.c_str());

  // Debug: Check message state after init
  std::cout << "=== DEBUG: After init - channel_length = "
            << message.channel_length << " ===" << std::endl;
  std::cout << "=== DEBUG: After init - channel = " << channel
            << " ===" << std::endl;
  std::cout << "=== DEBUG: After init - channel length should be = "
            << channel.length() << " ===" << std::endl;

  // Debug: Check if this is a memory alignment issue
  std::cout << "=== DEBUG: Checking memory alignment ===" << std::endl;
  std::cout << "message.stream_id = " << message.stream_id << std::endl;
  std::cout << "message.registration_id = " << message.registration_id
            << std::endl;
  std::cout << "message.header.correlation_id = "
            << message.header.correlation_id << std::endl;
  std::cout << "message.header.client_id = " << message.header.client_id
            << std::endl;

  // Check if channel_length is reading from wrong memory location
  std::uint8_t *raw_bytes = reinterpret_cast<std::uint8_t *>(&message);
  std::size_t expected_offset = sizeof(protocol::ControlMessageHeader) +
                                sizeof(std::int32_t) + sizeof(std::int64_t);
  std::int32_t *raw_channel_length =
      reinterpret_cast<std::int32_t *>(raw_bytes + expected_offset);
  std::cout << "=== DEBUG: Raw channel_length at expected offset = "
            << *raw_channel_length << " ===" << std::endl;
  std::cout.flush();

  // Send control message to driver
  send_control_message(message);

  // Create subscription object
  auto subscription =
      std::make_shared<Subscription>(channel, stream_id, registration_id);

  // Create a dummy image for testing
  try {
    auto log_buffer = std::make_unique<util::MemoryMappedFile>(
        util::PathUtils::join_path(context_.aeron_dir,
                                   "sub-" + std::to_string(registration_id)),
        64 * 1024, true);

    auto image = std::make_shared<Image>(
        static_cast<std::int32_t>(correlation_id & 0xFFFFFFFF), correlation_id,
        std::move(log_buffer));

    subscription->add_image(image);
    std::cout << "Created dummy image for subscription" << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Failed to create dummy image: " << e.what() << std::endl;
    // Continue without image for now
  }

  // Store subscription
  {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    subscriptions_[registration_id] = subscription;
  }

  std::cout << "Added subscription: channel=" << channel
            << ", stream=" << stream_id << ", registration=" << registration_id
            << std::endl;

  return subscription;
}

std::shared_ptr<ExclusivePublication>
Aeron::add_exclusive_publication(const std::string &channel,
                                 std::int32_t stream_id) {
  if (!connected_.load()) {
    throw std::runtime_error("Aeron client not connected");
  }

  std::int64_t correlation_id = next_correlation_id();
  std::int64_t registration_id = correlation_id;

  // Create publication message (same as regular publication for now)
  protocol::PublicationMessage message;
  message.init(correlation_id, client_id_, stream_id, 0, channel.c_str());

  // Send control message to driver
  send_control_message(message.header);

  // Create exclusive publication object
  std::int32_t session_id =
      static_cast<std::int32_t>(correlation_id & 0xFFFFFFFF);

  auto log_buffer = std::make_unique<util::MemoryMappedFile>(
      util::PathUtils::join_path(context_.aeron_dir,
                                 "excl-pub-" + std::to_string(registration_id)),
      64 * 1024, true);

  auto publication = std::make_shared<ExclusivePublication>(
      channel, stream_id, session_id, registration_id, std::move(log_buffer));

  std::cout << "Added exclusive publication: channel=" << channel
            << ", stream=" << stream_id << ", session=" << session_id
            << std::endl;

  return publication;
}

void Aeron::close_publication(std::shared_ptr<Publication> publication) {
  if (!publication) {
    return;
  }

  std::int64_t registration_id = publication->registration_id();

  // Remove from local storage
  {
    std::lock_guard<std::mutex> lock(publications_mutex_);
    publications_.erase(registration_id);
  }

  // Send remove message to driver
  // In a real implementation, we would send a REMOVE_PUBLICATION control
  // message

  publication->close();

  std::cout << "Closed publication: registration=" << registration_id
            << std::endl;
}

void Aeron::close_subscription(std::shared_ptr<Subscription> subscription) {
  if (!subscription) {
    return;
  }

  std::int64_t registration_id = subscription->registration_id();

  // Remove from local storage
  {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    subscriptions_.erase(registration_id);
  }

  // Send remove message to driver
  // In a real implementation, we would send a REMOVE_SUBSCRIPTION control
  // message

  subscription->close();

  std::cout << "Closed subscription: registration=" << registration_id
            << std::endl;
}

void Aeron::close() {
  if (!running_.load()) {
    return;
  }

  std::cout << "Closing Aeron client: " << client_id_ << std::endl;

  running_.store(false);
  connected_.store(false);

  // Stop conductor thread
  conductor_running_.store(false);
  if (conductor_thread_.joinable()) {
    conductor_thread_.join();
  }

  // Close all publications
  {
    std::lock_guard<std::mutex> lock(publications_mutex_);
    for (auto &pair : publications_) {
      pair.second->close();
    }
    publications_.clear();
  }

  // Close all subscriptions
  {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);
    for (auto &pair : subscriptions_) {
      pair.second->close();
    }
    subscriptions_.clear();
  }

  std::cout << "Aeron client closed" << std::endl;
}

void Aeron::send_control_message(
    const protocol::ControlMessageHeader &message) {
  if (!to_driver_buffer_) {
    std::cerr << "No to-driver buffer available" << std::endl;
    return;
  }

  std::uint8_t *buffer = to_driver_buffer_->memory();
  if (!buffer) {
    std::cerr << "Failed to get memory address for to-driver buffer"
              << std::endl;
    return;
  }

  // Write the message to the shared memory buffer
  // This is a simplified implementation - in the real Aeron, this would use
  // proper buffer management with read/write cursors

  std::memcpy(buffer, &message, message.length);

  // Log the message for debugging
  std::cout << "Sending control message: type="
            << static_cast<int>(message.type)
            << ", correlation_id=" << message.correlation_id << std::endl;
}

void Aeron::send_control_message(const protocol::PublicationMessage &message) {
  if (!to_driver_buffer_) {
    std::cerr << "No to-driver buffer available" << std::endl;
    return;
  }

  std::uint8_t *buffer = to_driver_buffer_->memory();
  if (!buffer) {
    std::cerr << "Failed to get memory address for to-driver buffer"
              << std::endl;
    return;
  }

  // Write the complete message (including channel data) to the shared memory
  // buffer
  std::memcpy(buffer, &message, message.header.length);

  // Log the message for debugging
  std::cout << "Sending publication message: type="
            << static_cast<int>(message.header.type)
            << ", correlation_id=" << message.header.correlation_id
            << ", channel=" << message.channel_data() << std::endl;
}

void Aeron::send_control_message(const protocol::SubscriptionMessage &message) {
  if (!to_driver_buffer_) {
    std::cerr << "No to-driver buffer available" << std::endl;
    return;
  }

  std::uint8_t *buffer = to_driver_buffer_->memory();
  if (!buffer) {
    std::cerr << "Failed to get memory address for to-driver buffer"
              << std::endl;
    return;
  }

  // Write the complete message (including channel data) to the shared memory
  // buffer
  std::memcpy(buffer, &message, message.header.length);

  // Log the message for debugging
  std::cout << "Sending subscription message: type="
            << static_cast<int>(message.header.type)
            << ", correlation_id=" << message.header.correlation_id
            << ", channel_length=" << message.channel_length
            << ", channel=" << message.channel_data() << std::endl;
}

void Aeron::process_driver_responses() {
  if (!to_client_buffer_) {
    return;
  }

  std::uint8_t *buffer = to_client_buffer_->memory();
  if (!buffer) {
    return;
  }

  // Read response messages from shared memory buffer
  // This is a simplified implementation - in the real Aeron, this would use
  // proper buffer management with read/write cursors

  // For now, we'll just check if there's any data in the buffer
  // In a real implementation, we would:
  // 1. Read the response header to determine message type and length
  // 2. Parse the full response message
  // 3. Dispatch to appropriate handler based on message type
  // 4. Update publication/subscription states
  // 5. Notify waiting threads

  // Check if there's a response message (simplified check)
  protocol::ResponseMessage *response =
      reinterpret_cast<protocol::ResponseMessage *>(buffer);

  // Simple check: if correlation_id is not 0, assume there's a response
  if (response && response->correlation_id != 0) {
    handle_driver_response(*response);

    // Clear the response (in a real implementation, this would update read
    // cursor)
    std::memset(buffer, 0, sizeof(*response));
  }
}

void Aeron::send_keepalive() {
  auto now = std::chrono::steady_clock::now();
  if (std::chrono::duration_cast<std::chrono::milliseconds>(
          now - last_keepalive_time_)
          .count() >= context_.keepalive_interval_ms) {

    protocol::ClientKeepaliveMessage keepalive;
    keepalive.init(next_correlation_id(), client_id_);

    send_control_message(keepalive.header);
    last_keepalive_time_ = now;
  }
}

std::int64_t Aeron::next_correlation_id() {
  return next_correlation_id_.fetch_add(1);
}

void Aeron::conductor_run() {
  std::cout << "Aeron conductor started for client: " << client_id_
            << std::endl;

  while (conductor_running_.load()) {
    try {
      // Process responses from media driver
      process_driver_responses();

      // Send keepalive messages
      send_keepalive();

      // Sleep for a short duration
      std::this_thread::sleep_for(std::chrono::milliseconds(10));

    } catch (const std::exception &e) {
      std::cerr << "Conductor error: " << e.what() << std::endl;

      if (context_.error_handler) {
        context_.error_handler(e);
      }
    }
  }

  std::cout << "Aeron conductor stopped for client: " << client_id_
            << std::endl;
}

void Aeron::handle_driver_response(const protocol::ResponseMessage &response) {
  std::cout << "Received driver response: type="
            << static_cast<int>(response.type)
            << ", correlation_id=" << response.correlation_id << std::endl;

  // Match the response to pending requests using correlation_id
  // In a real implementation, this would:
  // 1. Look up the pending request in a map using correlation_id
  // 2. Update publication/subscription states based on response
  // 3. Notify waiting threads
  // 4. Call appropriate callbacks

  switch (response.type) {
  case protocol::ResponseCode::ON_PUBLICATION_READY:
    handle_publication_ready_response(response);
    break;
  case protocol::ResponseCode::ON_SUBSCRIPTION_READY:
    handle_subscription_ready_response(response);
    break;
  case protocol::ResponseCode::ON_OPERATION_SUCCESS:
    handle_operation_success_response(response);
    break;
  case protocol::ResponseCode::ON_ERROR:
    handle_error_response(response);
    break;
  default:
    std::cout << "Unknown response type: " << static_cast<int>(response.type)
              << std::endl;
    break;
  }
}

void Aeron::handle_publication_ready_response(
    const protocol::ResponseMessage &response) {
  // Update publication state
  // In a real implementation, this would:
  // 1. Find the publication by correlation_id
  // 2. Update its state to READY
  // 3. Notify any waiting threads

  std::cout << "Publication ready: correlation_id=" << response.correlation_id
            << std::endl;
}

void Aeron::handle_subscription_ready_response(
    const protocol::ResponseMessage &response) {
  // Update subscription state
  // In a real implementation, this would:
  // 1. Find the subscription by correlation_id
  // 2. Update its state to READY
  // 3. Notify any waiting threads

  std::cout << "Subscription ready: correlation_id=" << response.correlation_id
            << std::endl;
}

void Aeron::handle_operation_success_response(
    const protocol::ResponseMessage &response) {
  // Handle successful operation response
  // In a real implementation, this would:
  // 1. Find the request by correlation_id
  // 2. Update the state to SUCCESS
  // 3. Notify waiting threads

  std::cout << "Operation success: correlation_id=" << response.correlation_id
            << ", registration_id=" << response.registration_id << std::endl;
}

void Aeron::handle_error_response(const protocol::ResponseMessage &response) {
  // Handle error response
  // In a real implementation, this would:
  // 1. Find the request by correlation_id
  // 2. Set error state
  // 3. Notify waiting threads with error

  std::cout << "Error response: correlation_id=" << response.correlation_id
            << std::endl;
}

} // namespace client
} // namespace aeron
