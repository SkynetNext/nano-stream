#pragma once

#include "context.h"
#include "publication.h"
#include "subscription.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace aeron {

// Forward declarations
class ClientConductor;

/**
 * Main Aeron client for publishing and subscribing to streams.
 * This is the primary API for interacting with Aeron.
 */
class Aeron {
public:
  /**
   * Create an Aeron instance with the provided context.
   */
  static std::shared_ptr<Aeron> connect(std::unique_ptr<Context> context) {
    // Create Aeron instance with simplified implementation
    auto aeron = std::shared_ptr<Aeron>(new Aeron(std::move(context)));
    return aeron;
  }

  /**
   * Create an Aeron instance with default context.
   */
  static std::shared_ptr<Aeron> connect() {
    auto context = std::make_unique<Context>();
    return connect(std::move(context));
  }

  /**
   * Destructor.
   */
  ~Aeron();

  // ========== Publication Methods ==========

  /**
   * Add a publication for a given channel and stream id.
   */
  std::shared_ptr<Publication> add_publication(const std::string &channel,
                                               std::int32_t stream_id);

  /**
   * Add an exclusive publication for a given channel and stream id.
   */
  std::shared_ptr<ExclusivePublication>
  add_exclusive_publication(const std::string &channel, std::int32_t stream_id);

  /**
   * Find an existing publication by registration id.
   */
  std::shared_ptr<Publication> find_publication(std::int64_t registration_id);

  /**
   * Find an existing exclusive publication by registration id.
   */
  std::shared_ptr<ExclusivePublication>
  find_exclusive_publication(std::int64_t registration_id);

  // ========== Subscription Methods ==========

  /**
   * Add a subscription for a given channel and stream id.
   */
  std::shared_ptr<Subscription> add_subscription(const std::string &channel,
                                                 std::int32_t stream_id);

  /**
   * Add a subscription with an available image handler.
   */
  std::shared_ptr<Subscription> add_subscription(
      const std::string &channel, std::int32_t stream_id,
      Context::available_image_handler_t available_image_handler,
      Context::unavailable_image_handler_t unavailable_image_handler = nullptr);

  /**
   * Find an existing subscription by registration id.
   */
  std::shared_ptr<Subscription> find_subscription(std::int64_t registration_id);

  // ========== Management Methods ==========

  /**
   * Close a publication and release its resources.
   */
  void close_publication(std::shared_ptr<Publication> publication);

  /**
   * Close a subscription and release its resources.
   */
  void close_subscription(std::shared_ptr<Subscription> subscription);

  /**
   * Close all publications and subscriptions.
   */
  void close() {
    is_closed_.store(true);
    // TODO: Cleanup publications and subscriptions
  }

  /**
   * Check if this client is closed.
   */
  bool is_closed() const { return is_closed_.load(); }

  /**
   * Get the context for this Aeron instance.
   */
  const Context &context() const { return *context_; }

  /**
   * Get the client id.
   */
  std::int64_t client_id() const { return context_->client_id(); }

  /**
   * Get the next correlation id.
   */
  std::int64_t next_correlation_id() {
    return correlation_id_counter_.fetch_add(1, std::memory_order_relaxed);
  }

  // ========== Async Operations ==========

  /**
   * Async result for add operations.
   */
  template <typename T> class AsyncAddResult {
  public:
    /**
     * Get the result if available.
     */
    std::shared_ptr<T> poll() { return result_.load(); }

    /**
     * Check if the operation is complete.
     */
    bool is_available() const { return result_.load() != nullptr; }

    /**
     * Get the registration id for this operation.
     */
    std::int64_t registration_id() const { return registration_id_; }

  private:
    friend class Aeron;
    AsyncAddResult(std::int64_t registration_id)
        : registration_id_(registration_id) {}

    std::int64_t registration_id_;
    std::atomic<std::shared_ptr<T>> result_;
  };

  /**
   * Asynchronously add a publication.
   */
  AsyncAddResult<Publication> async_add_publication(const std::string &channel,
                                                    std::int32_t stream_id);

  /**
   * Asynchronously add an exclusive publication.
   */
  AsyncAddResult<ExclusivePublication>
  async_add_exclusive_publication(const std::string &channel,
                                  std::int32_t stream_id);

  /**
   * Asynchronously add a subscription.
   */
  AsyncAddResult<Subscription>
  async_add_subscription(const std::string &channel, std::int32_t stream_id);

  // ========== Counter Access ==========

  /**
   * Get all counters from the media driver.
   */
  std::vector<std::pair<std::int32_t, std::string>> counters() const;

  /**
   * Get a counter value by its id.
   */
  std::int64_t counter_value(std::int32_t counter_id) const;

  // ========== Utility Methods ==========

  /**
   * Print configuration to a string.
   */
  std::string print_configuration() const;

  /**
   * Get the Aeron directory being used.
   */
  const std::string &aeron_directory() const { return context_->aeron_dir(); }

  /**
   * Get access to the client conductor for advanced operations.
   */
  ClientConductor &conductor() const { return *conductor_; }

private:
  /**
   * Private constructor.
   */
  explicit Aeron(std::unique_ptr<Context> context);

  /**
   * Initialize the client.
   */
  void initialize();

  /**
   * Ensure the media driver is active.
   */
  void await_media_driver();

  // Member variables
  std::unique_ptr<Context> context_;
  std::atomic<bool> is_closed_;
  std::atomic<std::int64_t> correlation_id_counter_;

  // Publication and subscription management
  std::unordered_map<std::int64_t, std::weak_ptr<Publication>> publications_;
  std::unordered_map<std::int64_t, std::weak_ptr<ExclusivePublication>>
      exclusive_publications_;
  std::unordered_map<std::int64_t, std::weak_ptr<Subscription>> subscriptions_;

  // Conductor thread management
  std::unique_ptr<class ClientConductor> conductor_;
  std::thread conductor_thread_;

  // Cleanup
  void cleanup_resources();
  void stop_conductor();

  // Initialization method to avoid circular dependency (implemented in
  // aeron_impl.h)
  void initialize_conductor();
};

/**
 * RAII helper for automatic Aeron instance management.
 */
class AeronRAII {
public:
  /**
   * Create and connect to Aeron.
   */
  explicit AeronRAII(std::unique_ptr<Context> context = nullptr)
      : aeron_(context ? Aeron::connect(std::move(context))
                       : Aeron::connect()) {}

  /**
   * Get the Aeron instance.
   */
  Aeron &operator*() const { return *aeron_; }

  /**
   * Get pointer to the Aeron instance.
   */
  Aeron *operator->() const { return aeron_.get(); }

  /**
   * Get shared pointer to the Aeron instance.
   */
  std::shared_ptr<Aeron> get() const { return aeron_; }

  /**
   * Destructor automatically closes Aeron.
   */
  ~AeronRAII() {
    if (aeron_) {
      aeron_->close();
    }
  }

private:
  std::shared_ptr<Aeron> aeron_;
};

} // namespace aeron
