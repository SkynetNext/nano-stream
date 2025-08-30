#pragma once

// Prevent winsock.h inclusion on Windows to avoid conflicts with winsock2.h
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_ // Prevent winsock.h from being included
#endif
#endif

#include <string>

namespace aeron {
namespace util {

/**
 * Cross-platform path utilities for Aeron.
 * Provides consistent path handling across different operating systems.
 */
class PathUtils {
public:
  /**
   * Get the default temporary directory for the current platform.
   *
   * @return Path to the temporary directory
   */
  static std::string get_temp_dir();

  /**
   * Get the current user's name.
   *
   * @return Username as string
   */
  static std::string get_username();

  /**
   * Get the default Aeron directory path for the current platform.
   *
   * @return Default Aeron directory path
   */
  static std::string get_default_aeron_dir();

  /**
   * Create a directory if it doesn't exist.
   *
   * @param path Directory path to create
   * @return true if successful, false otherwise
   */
  static bool create_directory(const std::string &path);

  /**
   * Check if a directory exists.
   *
   * @param path Directory path to check
   * @return true if directory exists, false otherwise
   */
  static bool directory_exists(const std::string &path);

  /**
   * Remove a directory and its contents.
   *
   * @param path Directory path to remove
   * @return true if successful, false otherwise
   */
  static bool remove_directory(const std::string &path);

  /**
   * Get the platform-specific file separator.
   *
   * @return File separator character
   */
  static char get_file_separator();

  /**
   * Join path components with the appropriate separator.
   *
   * @param components Path components to join
   * @return Joined path
   */
  template <typename... Args>
  static std::string join_path(Args &&...components);

private:
  static std::string join_path_impl();

  template <typename T, typename... Args>
  static std::string join_path_impl(T &&first, Args &&...rest);
};

// Template implementation
template <typename... Args>
std::string PathUtils::join_path(Args &&...components) {
  return join_path_impl(std::forward<Args>(components)...);
}

template <typename T, typename... Args>
std::string PathUtils::join_path_impl(T &&first, Args &&...rest) {
  std::string result = std::forward<T>(first);
  std::string rest_path = join_path_impl(std::forward<Args>(rest)...);

  if (!rest_path.empty()) {
    if (!result.empty() && result.back() != get_file_separator()) {
      result += get_file_separator();
    }
    result += rest_path;
  }

  return result;
}

} // namespace util
} // namespace aeron
