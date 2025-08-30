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
  send_control_message(message.header);

  // Create publication object
  // In a real implementation, we would wait for driver response and get actual
  // session_id
  std::int32_t session_id =
      static_cast<std::int32_t>(correlation_id & 0xFFFFFFFF);

  // Create dummy log buffer
  auto log_buffer = std::make_unique<util::MemoryMappedFile>(
      util::PathUtils::join_path(context_.aeron_dir,
                                 "pub-" + std::to_string(registration_id)),
      64 * 1024, true);

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
  message.init(correlation_id, client_id_, stream_id, registration_id,
               channel.c_str());

  // Send control message to driver
  send_control_message(message.header);

  // Create subscription object
  auto subscription =
      std::make_shared<Subscription>(channel, stream_id, registration_id);

  // Create a dummy image for testing
  auto log_buffer = std::make_unique<util::MemoryMappedFile>(
      util::PathUtils::join_path(context_.aeron_dir,
                                 "sub-" + std::to_string(registration_id)),
      64 * 1024, true);

  auto image = std::make_shared<Image>(
      static_cast<std::int32_t>(correlation_id & 0xFFFFFFFF), correlation_id,
      std::move(log_buffer));

  subscription->add_image(image);

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
  // In a real implementation, this would:
  // 1. Serialize the message to the to_driver_buffer_
  // 2. Signal the media driver that a new message is available
  // 3. Wait for response if needed

  // For now, just log the message
  std::cout << "Sending control message: type="
            << static_cast<int>(message.type)
            << ", correlation_id=" << message.correlation_id << std::endl;
}

void Aeron::process_driver_responses() {
  // In a real implementation, this would:
  // 1. Read responses from to_client_buffer_
  // 2. Parse and dispatch responses
  // 3. Update publication/subscription states
  // 4. Notify waiting threads

  // For now, this is a no-op
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
  // In a real implementation, this would:
  // 1. Match the response to pending requests using correlation_id
  // 2. Update publication/subscription states
  // 3. Notify waiting threads
  // 4. Call appropriate callbacks

  std::cout << "Received driver response: type="
            << static_cast<int>(response.type)
            << ", correlation_id=" << response.correlation_id << std::endl;
}

} // namespace client
} // namespace aeron
