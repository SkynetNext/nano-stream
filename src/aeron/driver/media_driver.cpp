#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#else
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "aeron/driver/media_driver.h"
#include "aeron/util/path_utils.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace aeron {
namespace driver {

// Global pointer for signal handling
static MediaDriver *g_media_driver = nullptr;

#ifdef _WIN32
BOOL WINAPI console_handler(DWORD signal) {
  if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
    if (g_media_driver) {
      g_media_driver->stop();
    }
    return TRUE;
  }
  return FALSE;
}
#else
void signal_handler(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    if (g_media_driver) {
      g_media_driver->stop();
    }
  }
}
#endif

std::unique_ptr<MediaDriver> MediaDriver::launch() {
  MediaDriverContext default_context;
  return launch(default_context);
}

std::unique_ptr<MediaDriver>
MediaDriver::launch(const MediaDriverContext &context) {
  auto driver = std::make_unique<MediaDriver>(context);
  driver->start();
  return driver;
}

MediaDriver::MediaDriver(const MediaDriverContext &context)
    : context_(context) {
  init_driver_directory();
  setup_signal_handlers();

  // Create shared memory buffers for counters and control
  const std::size_t counters_metadata_size = 1024 * 1024; // 1MB
  const std::size_t counters_values_size = 1024 * 1024;   // 1MB
  const std::size_t control_buffer_size = 1024 * 1024;    // 1MB

  try {
    auto counters_metadata_file = std::make_unique<util::MemoryMappedFile>(
        util::PathUtils::join_path(context_.aeron_dir, "counters-metadata"),
        counters_metadata_size, true);
    auto counters_values_file = std::make_unique<util::MemoryMappedFile>(
        util::PathUtils::join_path(context_.aeron_dir, "counters-values"),
        counters_values_size, true);

    // Create control communication buffers
    std::string to_driver_path =
        util::PathUtils::join_path(context_.aeron_dir, "to-driver");
    std::string to_client_path =
        util::PathUtils::join_path(context_.aeron_dir, "to-client");

    std::cout << "Creating to-driver buffer: " << to_driver_path << std::endl;
    std::cout << "Creating to-client buffer: " << to_client_path << std::endl;

    to_driver_buffer_ = std::make_unique<util::MemoryMappedFile>(
        to_driver_path, control_buffer_size, true);
    to_client_buffer_ = std::make_unique<util::MemoryMappedFile>(
        to_client_path, control_buffer_size, true);

    // Create counters manager
    counters_manager_ = std::make_unique<CountersManager>(
        counters_metadata_file->memory(), counters_values_file->memory(),
        counters_metadata_size, counters_values_size);

    // Create driver components
    log_buffer_manager_ = std::make_shared<LogBufferManager>();
    conductor_ = std::make_unique<Conductor>();
    sender_ = std::make_unique<Sender>();
    receiver_ = std::make_unique<Receiver>();

    // Set control buffers for conductor
    conductor_->set_control_buffers(std::move(to_driver_buffer_),
                                    std::move(to_client_buffer_));

    // Set log buffer manager for sender and receiver
    sender_->set_log_buffer_manager(log_buffer_manager_);
    receiver_->set_log_buffer_manager(log_buffer_manager_);

    std::cout << "MediaDriver initialized with directory: "
              << context_.aeron_dir << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Failed to initialize MediaDriver: " << e.what() << std::endl;
    throw;
  }
}

MediaDriver::~MediaDriver() {
  stop();
  cleanup_driver_directory();
  g_media_driver = nullptr;
}

void MediaDriver::start() {
  if (running_.load()) {
    return;
  }

  std::cout << "Starting MediaDriver..." << std::endl;

  try {
    // Start all components
    conductor_->start();
    sender_->start();
    receiver_->start();

    running_.store(true);
    g_media_driver = this;

    std::cout << "MediaDriver started successfully" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Failed to start MediaDriver: " << e.what() << std::endl;
    stop();
    throw;
  }
}

void MediaDriver::stop() {
  if (!running_.load()) {
    return;
  }

  std::cout << "Stopping MediaDriver..." << std::endl;

  shutdown_requested_.store(true);
  running_.store(false);

  // Stop all components in reverse order
  if (receiver_) {
    receiver_->stop();
  }

  if (sender_) {
    sender_->stop();
  }

  if (conductor_) {
    conductor_->stop();
  }

  std::cout << "MediaDriver stopped" << std::endl;
}

void MediaDriver::wait_for_termination() {
  while (running_.load() && !shutdown_requested_.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void MediaDriver::init_driver_directory() {
  try {
    // Create aeron directory if it doesn't exist
    if (!util::PathUtils::directory_exists(context_.aeron_dir)) {
      if (!util::PathUtils::create_directory(context_.aeron_dir)) {
        std::cerr << "Failed to create Aeron directory: " << context_.aeron_dir
                  << std::endl;
        throw std::runtime_error("Failed to create Aeron directory");
      }
    }

    // Create driver PID file
    std::string pid_file =
        util::PathUtils::join_path(context_.aeron_dir, "driver.pid");
    std::ofstream pid_stream(pid_file);
    if (pid_stream.is_open()) {
#ifdef _WIN32
      pid_stream << GetCurrentProcessId() << std::endl;
#else
      pid_stream << getpid() << std::endl;
#endif
      pid_stream.close();
    }

    // Create version file
    std::string version_file =
        util::PathUtils::join_path(context_.aeron_dir, "version.txt");
    std::ofstream version_stream(version_file);
    if (version_stream.is_open()) {
      version_stream << "aeron-cpp-1.0.0" << std::endl;
      version_stream.close();
    }

    std::cout << "Driver directory initialized: " << context_.aeron_dir
              << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Failed to initialize driver directory: " << e.what()
              << std::endl;
    throw;
  }
}

void MediaDriver::cleanup_driver_directory() {
  try {
    // Remove PID file
    std::string pid_file =
        util::PathUtils::join_path(context_.aeron_dir, "driver.pid");
    std::filesystem::remove(pid_file);

    std::cout << "Driver directory cleaned up" << std::endl;

  } catch (const std::exception &e) {
    std::cerr << "Error cleaning up driver directory: " << e.what()
              << std::endl;
  }
}

void MediaDriver::setup_signal_handlers() {
#ifdef _WIN32
  if (!SetConsoleCtrlHandler(console_handler, TRUE)) {
    std::cerr << "Warning: Failed to set console control handler" << std::endl;
  }
#else
  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGINT, &sa, nullptr) == -1) {
    std::cerr << "Warning: Failed to set SIGINT handler" << std::endl;
  }

  if (sigaction(SIGTERM, &sa, nullptr) == -1) {
    std::cerr << "Warning: Failed to set SIGTERM handler" << std::endl;
  }
#endif
}

} // namespace driver
} // namespace aeron
