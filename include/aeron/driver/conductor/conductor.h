#pragma once

#include "../../protocol/control_protocol.h"
#include "../../util/memory_mapped_file.h"
#include "../log_buffer_manager.h"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace aeron {
namespace driver {

/**
 * The Conductor manages client connections and coordinates between
 * the Sender and Receiver threads. It handles control messages from
 * clients and manages publication/subscription lifecycle.
 */
class Conductor {
public:
  /**
   * Constructor.
   */
  Conductor();

  /**
   * Destructor.
   */
  ~Conductor();

  /**
   * Start the conductor thread.
   */
  void start();

  /**
   * Stop the conductor thread.
   */
  void stop();

  /**
   * Check if the conductor is running.
   */
  bool is_running() const { return running_.load(); }

  /**
   * Set control buffers (called by Media Driver).
   */
  void
  set_control_buffers(std::unique_ptr<util::MemoryMappedFile> request_buffer,
                      std::unique_ptr<util::MemoryMappedFile> response_buffer);

  /**
   * Set log buffer manager (called by Media Driver).
   */
  void set_log_buffer_manager(std::shared_ptr<LogBufferManager> manager);

  /**
   * Set aeron directory (called by Media Driver).
   */
  void set_aeron_directory(const std::string &aeron_dir);

  /**
   * Set sender reference (called by Media Driver).
   */
  void set_sender(std::unique_ptr<class Sender> &sender);

  /**
   * Set receiver reference (called by Media Driver).
   */
  void set_receiver(std::unique_ptr<class Receiver> &receiver);

  /**
   * Main conductor loop (runs in separate thread).
   */
  void run();

private:
  struct ClientSession {
    std::int64_t client_id;
    std::int64_t last_keepalive_time;
    std::unordered_map<std::int64_t, std::int32_t>
        publications; // registration_id -> session_id
    std::unordered_map<std::int64_t, std::int32_t>
        subscriptions; // registration_id -> stream_id
  };

  struct Publication {
    std::int32_t session_id;
    std::int32_t stream_id;
    std::string channel;
    std::int64_t registration_id;
    std::int64_t client_id;
  };

  struct Subscription {
    std::int32_t stream_id;
    std::string channel;
    std::int64_t registration_id;
    std::int64_t client_id;
  };

  /**
   * Process incoming control messages from clients.
   */
  void process_control_messages();

  /**
   * Read control message header from shared memory buffer.
   */
  bool read_control_message_header(protocol::ControlMessageHeader &header);

  /**
   * Handle add publication message from shared memory.
   */
  void handle_add_publication_message();

  /**
   * Handle remove publication message from shared memory.
   */
  void handle_remove_publication_message();

  /**
   * Handle add subscription message from shared memory.
   */
  void handle_add_subscription_message();

  /**
   * Handle remove subscription message from shared memory.
   */
  void handle_remove_subscription_message();

  /**
   * Handle client keepalive message from shared memory.
   */
  void handle_client_keepalive_message();

  /**
   * Handle add publication request.
   */
  void handle_add_publication(const protocol::PublicationMessage &message);

  /**
   * Handle remove publication request.
   */
  void handle_remove_publication(std::int64_t registration_id,
                                 std::int64_t client_id);

  /**
   * Handle add subscription request.
   */
  void handle_add_subscription(const protocol::SubscriptionMessage &message);

  /**
   * Handle remove subscription request.
   */
  void handle_remove_subscription(std::int64_t registration_id,
                                  std::int64_t client_id);

  /**
   * Handle client keepalive.
   */
  void handle_client_keepalive(std::int64_t client_id);

  /**
   * Check for client timeouts and cleanup.
   */
  void check_client_timeouts();

  /**
   * Send response to client.
   */
  void send_response(const protocol::ResponseMessage &response);

  /**
   * Generate unique session ID.
   */
  std::int32_t generate_session_id();

  /**
   * Generate unique registration ID.
   */
  std::int64_t generate_registration_id();

  // Thread control
  std::atomic<bool> running_{false};
  std::thread conductor_thread_;

  // Client management
  std::mutex clients_mutex_;
  std::unordered_map<std::int64_t, std::unique_ptr<ClientSession>> clients_;

  // Publication/Subscription management
  std::mutex publications_mutex_;
  std::mutex subscriptions_mutex_;
  std::unordered_map<std::int64_t, std::unique_ptr<Publication>> publications_;
  std::unordered_map<std::int64_t, std::unique_ptr<Subscription>>
      subscriptions_;

  // ID generation
  std::atomic<std::int32_t> next_session_id_{1};
  std::atomic<std::int64_t> next_registration_id_{1};

  // Control message communication (shared memory)
  std::unique_ptr<util::MemoryMappedFile> control_request_buffer_;
  std::unique_ptr<util::MemoryMappedFile> control_response_buffer_;
  std::shared_ptr<LogBufferManager> log_buffer_manager_;
  std::unique_ptr<class Sender> *sender_;
  std::unique_ptr<class Receiver> *receiver_;

  // Configuration
  std::string aeron_dir_;

  // Configuration
  static constexpr std::int64_t CLIENT_TIMEOUT_MS = 5000;
  static constexpr std::int32_t CONDUCTOR_TICK_DURATION_MS = 10;
  static constexpr std::size_t CONTROL_BUFFER_SIZE = 1024 * 1024; // 1MB
};

} // namespace driver
} // namespace aeron
