#pragma once

#include <chrono>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace aeron {

// Forward declarations
class Publication;
class ExclusivePublication;
class Subscription;
class Image;

/**
 * Configuration context for Aeron client.
 * Contains all the configuration parameters and handlers.
 */
class Context {
public:
  /**
   * Default constructor with standard settings.
   */
  Context()
      : aeron_dir_("/tmp/aeron"), // Default directory
        use_conductor_agent_invoker_(false),
        keep_alive_interval_(std::chrono::seconds(30)),
        inter_service_timeout_(std::chrono::seconds(10)),
        publication_connection_timeout_(std::chrono::seconds(5)),
        resource_linger_timeout_(std::chrono::seconds(5)),
        publication_term_buffer_length_(16 * 1024 * 1024), // 16MB
        ipc_term_buffer_length_(64 * 1024 * 1024),         // 64MB
        client_name_("aeron-client"), // Default client name
        client_id_(1),                // Default client ID
        pre_touch_mapped_memory_(false) {
    // Set default idle strategy
    idle_strategy_ = []() { std::this_thread::yield(); };
  }

  /**
   * Copy constructor.
   */
  Context(const Context &other) = default;

  /**
   * Move constructor.
   */
  Context(Context &&other) = default;

  /**
   * Assignment operator.
   */
  Context &operator=(const Context &other) = default;

  /**
   * Move assignment operator.
   */
  Context &operator=(Context &&other) = default;

  /**
   * Destructor.
   */
  ~Context() = default;

  // ========== Directory Settings ==========

  /**
   * Set the directory for the Media Driver.
   */
  Context &aeron_dir(const std::string &dir) {
    aeron_dir_ = dir;
    return *this;
  }

  /**
   * Get the directory for the Media Driver.
   */
  const std::string &aeron_dir() const { return aeron_dir_; }

  // ========== Threading Settings ==========

  /**
   * Set the idle strategy for the client conductor thread.
   */
  Context &idle_strategy(std::function<void()> strategy) {
    idle_strategy_ = std::move(strategy);
    return *this;
  }

  /**
   * Get the idle strategy for the client conductor thread.
   */
  const std::function<void()> &idle_strategy() const { return idle_strategy_; }

  /**
   * Set whether to use dedicated thread for the conductor.
   */
  Context &use_conductor_agent_invoker(bool use_invoker) {
    use_conductor_agent_invoker_ = use_invoker;
    return *this;
  }

  /**
   * Get whether to use dedicated thread for the conductor.
   */
  bool use_conductor_agent_invoker() const {
    return use_conductor_agent_invoker_;
  }

  // ========== Timeout Settings ==========

  /**
   * Set the timeout for keep alive messages.
   */
  Context &keep_alive_interval(std::chrono::nanoseconds interval) {
    keep_alive_interval_ = interval;
    return *this;
  }

  /**
   * Get the timeout for keep alive messages.
   */
  std::chrono::nanoseconds keep_alive_interval() const {
    return keep_alive_interval_;
  }

  /**
   * Set the timeout for inter-service messages.
   */
  Context &inter_service_timeout(std::chrono::nanoseconds timeout) {
    inter_service_timeout_ = timeout;
    return *this;
  }

  /**
   * Get the timeout for inter-service messages.
   */
  std::chrono::nanoseconds inter_service_timeout() const {
    return inter_service_timeout_;
  }

  /**
   * Set the timeout for publication connection.
   */
  Context &publication_connection_timeout(std::chrono::nanoseconds timeout) {
    publication_connection_timeout_ = timeout;
    return *this;
  }

  /**
   * Get the timeout for publication connection.
   */
  std::chrono::nanoseconds publication_connection_timeout() const {
    return publication_connection_timeout_;
  }

  // ========== Buffer Settings ==========

  /**
   * Set the default term buffer length.
   */
  Context &publication_term_buffer_length(std::int32_t length) {
    publication_term_buffer_length_ = length;
    return *this;
  }

  /**
   * Get the default term buffer length.
   */
  std::int32_t publication_term_buffer_length() const {
    return publication_term_buffer_length_;
  }

  /**
   * Set the default IPC term buffer length.
   */
  Context &ipc_term_buffer_length(std::int32_t length) {
    ipc_term_buffer_length_ = length;
    return *this;
  }

  /**
   * Get the default IPC term buffer length.
   */
  std::int32_t ipc_term_buffer_length() const {
    return ipc_term_buffer_length_;
  }

  // ========== Event Handlers ==========

  /**
   * Handler for when a new publication is available.
   */
  using available_image_handler_t =
      std::function<void(std::shared_ptr<Image> image)>;

  /**
   * Set the handler for when a new image becomes available.
   */
  Context &available_image_handler(available_image_handler_t handler) {
    available_image_handler_ = std::move(handler);
    return *this;
  }

  /**
   * Get the handler for when a new image becomes available.
   */
  const available_image_handler_t &available_image_handler() const {
    return available_image_handler_;
  }

  /**
   * Handler for when an image becomes unavailable.
   */
  using unavailable_image_handler_t =
      std::function<void(std::shared_ptr<Image> image)>;

  /**
   * Set the handler for when an image becomes unavailable.
   */
  Context &unavailable_image_handler(unavailable_image_handler_t handler) {
    unavailable_image_handler_ = std::move(handler);
    return *this;
  }

  /**
   * Get the handler for when an image becomes unavailable.
   */
  const unavailable_image_handler_t &unavailable_image_handler() const {
    return unavailable_image_handler_;
  }

  /**
   * Handler for error conditions.
   */
  using error_handler_t = std::function<void(const std::exception &exception)>;

  /**
   * Set the error handler.
   */
  Context &error_handler(error_handler_t handler) {
    error_handler_ = std::move(handler);
    return *this;
  }

  /**
   * Get the error handler.
   */
  const error_handler_t &error_handler() const { return error_handler_; }

  /**
   * Handler for close events.
   */
  using close_handler_t = std::function<void()>;

  /**
   * Set the close handler.
   */
  Context &close_handler(close_handler_t handler) {
    close_handler_ = std::move(handler);
    return *this;
  }

  /**
   * Get the close handler.
   */
  const close_handler_t &close_handler() const { return close_handler_; }

  // ========== Client Settings ==========

  /**
   * Set the client name.
   */
  Context &client_name(const std::string &name) {
    client_name_ = name;
    return *this;
  }

  /**
   * Get the client name.
   */
  const std::string &client_name() const { return client_name_; }

  /**
   * Set the client id.
   */
  Context &client_id(std::int64_t id) {
    client_id_ = id;
    return *this;
  }

  /**
   * Get the client id.
   */
  std::int64_t client_id() const { return client_id_; }

  // ========== Resource Management ==========

  /**
   * Set whether to pre-touch mapped memory.
   */
  Context &pre_touch_mapped_memory(bool pre_touch) {
    pre_touch_mapped_memory_ = pre_touch;
    return *this;
  }

  /**
   * Get whether to pre-touch mapped memory.
   */
  bool pre_touch_mapped_memory() const { return pre_touch_mapped_memory_; }

  /**
   * Set the resource linger timeout.
   */
  Context &resource_linger_timeout(std::chrono::nanoseconds timeout) {
    resource_linger_timeout_ = timeout;
    return *this;
  }

  /**
   * Get the resource linger timeout.
   */
  std::chrono::nanoseconds resource_linger_timeout() const {
    return resource_linger_timeout_;
  }

  // ========== Validation ==========

  /**
   * Validate the context settings.
   */
  void conclude();

  /**
   * Create a copy of this context.
   */
  std::unique_ptr<Context> clone() const;

private:
  // Directory settings
  std::string aeron_dir_;

  // Threading
  std::function<void()> idle_strategy_;
  bool use_conductor_agent_invoker_;

  // Timeouts
  std::chrono::nanoseconds keep_alive_interval_;
  std::chrono::nanoseconds inter_service_timeout_;
  std::chrono::nanoseconds publication_connection_timeout_;
  std::chrono::nanoseconds resource_linger_timeout_;

  // Buffer settings
  std::int32_t publication_term_buffer_length_;
  std::int32_t ipc_term_buffer_length_;

  // Handlers
  available_image_handler_t available_image_handler_;
  unavailable_image_handler_t unavailable_image_handler_;
  error_handler_t error_handler_;
  close_handler_t close_handler_;

  // Client settings
  std::string client_name_;
  std::int64_t client_id_;

  // Resource management
  bool pre_touch_mapped_memory_;

  // Helper methods
  void set_defaults();
  std::string generate_random_dir() const;
  void validate_term_buffer_length(std::int32_t length) const;
};

/**
 * Builder pattern helper for Context.
 */
class ContextBuilder {
public:
  /**
   * Build a context with the specified settings.
   */
  static std::unique_ptr<Context> build() {
    return std::make_unique<Context>();
  }

  /**
   * Build a context for testing with minimal settings.
   */
  static std::unique_ptr<Context> build_for_testing();

  /**
   * Build a context for embedded use.
   */
  static std::unique_ptr<Context> build_embedded();
};

} // namespace aeron
