#pragma once

#include "../protocol/control_protocol.h"
#include "../util/memory_mapped_file.h"
#include "../util/path_utils.h"
#include "publication.h"
#include "subscription.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace aeron {
namespace client {

/**
 * Aeron client context configuration.
 */
struct AeronContext {
  std::string aeron_dir = util::PathUtils::get_default_aeron_dir();
  std::int64_t client_id = 0; // 0 = auto-generate
  std::int32_t media_driver_timeout_ms = 10000;
  std::int32_t keepalive_interval_ms = 500;
  std::int32_t inter_service_timeout_ms = 10000;
  std::int32_t publication_connection_timeout_ms = 5000;
  bool pre_touch_mapped_memory = false;

  // Error handling
  std::function<void(const std::exception &)> error_handler;
  std::function<void(std::int64_t)> on_new_publication;
  std::function<void(std::int64_t)> on_new_subscription;
  std::function<void(std::int64_t)> on_available_image;
  std::function<void(std::int64_t)> on_unavailable_image;
};

/**
 * Main Aeron client interface.
 * Provides access to publications and subscriptions for messaging.
 */
class Aeron {
public:
  /**
   * Connect to the Media Driver with default context.
   */
  static std::shared_ptr<Aeron> connect();

  /**
   * Connect to the Media Driver with custom context.
   */
  static std::shared_ptr<Aeron> connect(const AeronContext &context);

  /**
   * Constructor.
   */
  explicit Aeron(const AeronContext &context);

  /**
   * Destructor.
   */
  ~Aeron();

  // Non-copyable
  Aeron(const Aeron &) = delete;
  Aeron &operator=(const Aeron &) = delete;

  /**
   * Add a publication for sending messages.
   *
   * @param channel Channel URI (e.g., "aeron:udp?endpoint=localhost:40456")
   * @param stream_id Stream identifier
   * @return Publication instance
   */
  std::shared_ptr<Publication> add_publication(const std::string &channel,
                                               std::int32_t stream_id);

  /**
   * Add a subscription for receiving messages.
   *
   * @param channel Channel URI (e.g., "aeron:udp?endpoint=localhost:40456")
   * @param stream_id Stream identifier
   * @return Subscription instance
   */
  std::shared_ptr<Subscription> add_subscription(const std::string &channel,
                                                 std::int32_t stream_id);

  /**
   * Add an exclusive publication (single publisher).
   *
   * @param channel Channel URI
   * @param stream_id Stream identifier
   * @return ExclusivePublication instance
   */
  std::shared_ptr<ExclusivePublication>
  add_exclusive_publication(const std::string &channel, std::int32_t stream_id);

  /**
   * Close a publication.
   *
   * @param publication Publication to close
   */
  void close_publication(std::shared_ptr<Publication> publication);

  /**
   * Close a subscription.
   *
   * @param subscription Subscription to close
   */
  void close_subscription(std::shared_ptr<Subscription> subscription);

  /**
   * Get the client ID.
   */
  std::int64_t client_id() const { return client_id_; }

  /**
   * Get the context.
   */
  const AeronContext &context() const { return context_; }

  /**
   * Check if connected to Media Driver.
   */
  bool is_connected() const { return connected_.load(); }

  /**
   * Close the Aeron client.
   */
  void close();

private:
  /**
   * Initialize connection to Media Driver.
   */
  void init_connection();

  /**
   * Send control message to Media Driver.
   */
  void send_control_message(const protocol::ControlMessageHeader &message);

  /**
   * Process responses from Media Driver.
   */
  void process_driver_responses();

  /**
   * Send keepalive to Media Driver.
   */
  void send_keepalive();

  /**
   * Generate unique correlation ID.
   */
  std::int64_t next_correlation_id();

  /**
   * Client conductor thread main loop.
   */
  void conductor_run();

  /**
   * Handle driver response.
   */
  void handle_driver_response(const protocol::ResponseMessage &response);

  AeronContext context_;
  std::int64_t client_id_;
  std::atomic<bool> connected_{false};
  std::atomic<bool> running_{false};

  // Communication with Media Driver
  std::unique_ptr<util::MemoryMappedFile> to_driver_buffer_;
  std::unique_ptr<util::MemoryMappedFile> to_client_buffer_;

  // Publications and subscriptions
  std::mutex publications_mutex_;
  std::mutex subscriptions_mutex_;
  std::unordered_map<std::int64_t, std::shared_ptr<Publication>> publications_;
  std::unordered_map<std::int64_t, std::shared_ptr<Subscription>>
      subscriptions_;

  // Correlation tracking
  std::atomic<std::int64_t> next_correlation_id_{1};
  std::mutex pending_requests_mutex_;
  std::unordered_map<std::int64_t, std::condition_variable> pending_requests_;
  std::unordered_map<std::int64_t, protocol::ResponseMessage> responses_;

  // Client conductor thread
  std::thread conductor_thread_;
  std::atomic<bool> conductor_running_{false};

  // Keepalive
  std::chrono::steady_clock::time_point last_keepalive_time_;

  static constexpr std::size_t CONTROL_BUFFER_SIZE = 1024 * 1024; // 1MB
};

} // namespace client
} // namespace aeron
