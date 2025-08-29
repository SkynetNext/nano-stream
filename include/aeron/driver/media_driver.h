#pragma once

#include "../concurrent/atomic_buffer.h"
#include "../util/memory_mapped_file.h"
#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace aeron {
namespace driver {

// Forward declarations
class DriverConductor;
class Receiver;
class Sender;
class ClientCommandAdapter;
class DriverContext;

/**
 * Threading mode for the Media Driver.
 * Based on Aeron's ThreadingMode enum.
 */
enum class ThreadingMode {
  /**
   * Single threaded driver with all agents running on the same thread.
   */
  SHARED,

  /**
   * Each agent runs on its own thread with shared network polling.
   */
  SHARED_NETWORK,

  /**
   * Each agent runs on its own dedicated thread.
   */
  DEDICATED,

  /**
   * Agents run via an invoker that can be called from client threads.
   */
  INVOKER
};

/**
 * Configuration context for the Media Driver.
 * Contains all configuration parameters and factory methods.
 */
class DriverContext {
public:
  /**
   * Default constructor with standard settings.
   */
  DriverContext()
      : aeron_dir_("/tmp/aeron-driver"), threading_mode_(ThreadingMode::SHARED),
        conductor_buffer_length_(1024 * 1024),
        publication_term_buffer_length_(16 * 1024 * 1024),
        ipc_publication_term_buffer_length_(64 * 1024 * 1024),
        socket_rcvbuf_length_(128 * 1024), socket_sndbuf_length_(128 * 1024),
        mtu_length_(1408), driver_timeout_ms_(10000),
        client_liveness_timeout_ns_(std::chrono::seconds(10).count()) {
    // Default initialization complete
  }

  /**
   * Destructor.
   */
  ~DriverContext() = default;

  // ========== Directory and File Settings ==========

  /**
   * Set the directory for storing driver files.
   */
  DriverContext &aeron_dir(const std::string &dir) {
    aeron_dir_ = dir;
    return *this;
  }

  /**
   * Get the directory for storing driver files.
   */
  const std::string &aeron_dir() const { return aeron_dir_; }

  // ========== Threading Configuration ==========

  /**
   * Set the threading mode for the driver.
   */
  DriverContext &threading_mode(ThreadingMode mode) {
    threading_mode_ = mode;
    return *this;
  }

  /**
   * Get the threading mode for the driver.
   */
  ThreadingMode threading_mode() const { return threading_mode_; }

  // ========== Buffer Configuration ==========

  /**
   * Set the conductor buffer length.
   */
  DriverContext &conductor_buffer_length(std::int32_t length) {
    conductor_buffer_length_ = length;
    return *this;
  }

  /**
   * Get the conductor buffer length.
   */
  std::int32_t conductor_buffer_length() const {
    return conductor_buffer_length_;
  }

  /**
   * Set the default publication term buffer length.
   */
  DriverContext &publication_term_buffer_length(std::int32_t length) {
    publication_term_buffer_length_ = length;
    return *this;
  }

  /**
   * Get the default publication term buffer length.
   */
  std::int32_t publication_term_buffer_length() const {
    return publication_term_buffer_length_;
  }

  /**
   * Set the IPC publication term buffer length.
   */
  DriverContext &ipc_publication_term_buffer_length(std::int32_t length) {
    ipc_publication_term_buffer_length_ = length;
    return *this;
  }

  /**
   * Get the IPC publication term buffer length.
   */
  std::int32_t ipc_publication_term_buffer_length() const {
    return ipc_publication_term_buffer_length_;
  }

  // ========== Network Configuration ==========

  /**
   * Set the socket receive buffer length.
   */
  DriverContext &socket_rcvbuf_length(std::int32_t length) {
    socket_rcvbuf_length_ = length;
    return *this;
  }

  /**
   * Get the socket receive buffer length.
   */
  std::int32_t socket_rcvbuf_length() const { return socket_rcvbuf_length_; }

  /**
   * Set the socket send buffer length.
   */
  DriverContext &socket_sndbuf_length(std::int32_t length) {
    socket_sndbuf_length_ = length;
    return *this;
  }

  /**
   * Get the socket send buffer length.
   */
  std::int32_t socket_sndbuf_length() const { return socket_sndbuf_length_; }

  /**
   * Set the MTU length.
   */
  DriverContext &mtu_length(std::int32_t length) {
    mtu_length_ = length;
    return *this;
  }

  /**
   * Get the MTU length.
   */
  std::int32_t mtu_length() const { return mtu_length_; }

  // ========== Timing Configuration ==========

  /**
   * Set the driver timeout in milliseconds.
   */
  DriverContext &driver_timeout_ms(std::int64_t timeout) {
    driver_timeout_ms_ = timeout;
    return *this;
  }

  /**
   * Get the driver timeout in milliseconds.
   */
  std::int64_t driver_timeout_ms() const { return driver_timeout_ms_; }

  /**
   * Set the client liveness timeout in nanoseconds.
   */
  DriverContext &client_liveness_timeout_ns(std::int64_t timeout) {
    client_liveness_timeout_ns_ = timeout;
    return *this;
  }

  /**
   * Get the client liveness timeout in nanoseconds.
   */
  std::int64_t client_liveness_timeout_ns() const {
    return client_liveness_timeout_ns_;
  }

  // ========== Error Handling ==========

  /**
   * Set the error handler.
   */
  DriverContext &
  error_handler(std::function<void(const std::exception &)> handler) {
    error_handler_ = std::move(handler);
    return *this;
  }

  /**
   * Get the error handler.
   */
  const std::function<void(const std::exception &)> &error_handler() const {
    return error_handler_;
  }

  // ========== Initialization ==========

  /**
   * Conclude the context and prepare for use.
   */
  void conclude();

private:
  // Directory settings
  std::string aeron_dir_;

  // Threading configuration
  ThreadingMode threading_mode_;

  // Buffer configuration
  std::int32_t conductor_buffer_length_;
  std::int32_t publication_term_buffer_length_;
  std::int32_t ipc_publication_term_buffer_length_;

  // Network configuration
  std::int32_t socket_rcvbuf_length_;
  std::int32_t socket_sndbuf_length_;
  std::int32_t mtu_length_;

  // Timing configuration
  std::int64_t driver_timeout_ms_;
  std::int64_t client_liveness_timeout_ns_;

  // Error handling
  std::function<void(const std::exception &)> error_handler_;

  // Helper methods
  void set_defaults();
  void validate_configuration();
  std::string ensure_aeron_directory();
};

/**
 * Media Driver for Aeron messaging.
 * This is the core component that handles all transport and protocol logic.
 */
class MediaDriver {
public:
  /**
   * Launch a Media Driver with the given context.
   */
  static std::unique_ptr<MediaDriver>
  launch(std::unique_ptr<DriverContext> context) {
    auto driver = std::make_unique<MediaDriver>(std::move(context));
    driver->initialize();
    return driver;
  }

  /**
   * Launch a Media Driver with default context.
   */
  static std::unique_ptr<MediaDriver> launch();

  /**
   * Constructor for MediaDriver.
   */
  explicit MediaDriver(std::unique_ptr<DriverContext> context);

  /**
   * Destructor - stops all agents and cleans up resources.
   */
  ~MediaDriver();

  /**
   * Non-copyable.
   */
  MediaDriver(const MediaDriver &) = delete;
  MediaDriver &operator=(const MediaDriver &) = delete;

  /**
   * Close the media driver and clean up resources.
   */
  void close() {
    if (!is_closed_.exchange(true)) {
      running_.store(false);
      stop_agents();
    }
  }

  /**
   * Check if the media driver is closed.
   */
  bool is_closed() const { return is_closed_.load(); }

  /**
   * Get the context for this media driver.
   */
  const DriverContext &context() const { return *context_; }

  /**
   * Get the Aeron directory path.
   */
  const std::string &aeron_directory() const { return context_->aeron_dir(); }

  /**
   * Add work for the driver to perform.
   * Returns the amount of work performed.
   */
  int do_work();

  /**
   * Idle strategy for when no work is available.
   */
  void on_close();

  /**
   * Agent runner for managing agent lifecycle in different threading modes.
   */
  class AgentRunner {
  public:
    /**
     * Interface for agents that can be run by the runner.
     */
    class Agent {
    public:
      virtual ~Agent() = default;
      virtual int do_work() = 0;
      virtual void on_start() {}
      virtual void on_close() {}
      virtual const std::string &role_name() const = 0;
    };

    /**
     * Constructor for AgentRunner.
     */
    AgentRunner(std::unique_ptr<Agent> agent,
                std::function<void(const std::exception &)> error_handler);

    /**
     * Destructor.
     */
    ~AgentRunner();

    /**
     * Start the agent runner.
     */
    void start();

    /**
     * Stop the agent runner.
     */
    void stop();

    /**
     * Check if the runner is running.
     */
    bool is_running() const { return running_.load(); }

    /**
     * Do work (for invoker mode).
     */
    int do_work();

  private:
    std::unique_ptr<Agent> agent_;
    std::function<void(const std::exception &)> error_handler_;
    std::atomic<bool> running_;
    std::thread runner_thread_;

    // Main agent loop
    void run();
  };

private:
  std::unique_ptr<DriverContext> context_;
  std::atomic<bool> is_closed_;

  // Core components (initialized in implementation file to avoid incomplete
  // types)
  std::unique_ptr<DriverConductor> conductor_;
  std::unique_ptr<Receiver> receiver_;
  std::unique_ptr<Sender> sender_;

  // Threading support
  std::vector<std::thread> agent_threads_;
  std::atomic<bool> running_;

  // Shared memory for command and control
  std::unique_ptr<util::MemoryMappedFile> cnc_file_;
  concurrent::AtomicBuffer cnc_buffer_;

  // Agent runners for different threading modes
  std::vector<std::unique_ptr<AgentRunner>> agent_runners_;

  // Initialization methods
  void initialize() {
    // Simplified initialization
    create_cnc_file();
    start_agents();
  }

  void create_cnc_file() {
    // Simplified - no actual file creation yet
  }

  void start_agents() {
    running_.store(true);
    // Simplified - no actual agents yet
  }

  void stop_agents() {
    running_.store(false);
    // Wait for agent threads to finish
    for (auto &thread : agent_threads_) {
      if (thread.joinable()) {
        thread.join();
      }
    }
  }

  // Threading mode specific initialization
  void initialize_shared_threading();
  void initialize_shared_network_threading();
  void initialize_dedicated_threading();
  void initialize_invoker_threading();

  // Work distribution methods
  int do_conductor_work();
  int do_receiver_work();
  int do_sender_work();
};

/**
 * Factory methods for creating media drivers with common configurations.
 */
namespace media_driver_factory {

/**
 * Create a low-latency media driver configuration.
 */
std::unique_ptr<DriverContext> low_latency_config();

/**
 * Create a high-throughput media driver configuration.
 */
std::unique_ptr<DriverContext> high_throughput_config();

/**
 * Create a development/testing media driver configuration.
 */
std::unique_ptr<DriverContext> development_config();

} // namespace media_driver_factory

} // namespace driver
} // namespace aeron
