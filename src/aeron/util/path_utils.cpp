#include "aeron/util/path_utils.h"

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>

#else
#include <cstdlib>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#endif

#include <iostream>

namespace aeron {
namespace util {

std::string PathUtils::get_temp_dir() {
#ifdef _WIN32
  char temp_path[MAX_PATH + 1];
  if (GetTempPath(MAX_PATH, temp_path) > 0) {
    std::string path(temp_path);
    // Remove trailing backslash if present
    if (!path.empty() && (path.back() == '\\' || path.back() == '/')) {
      path.pop_back();
    }
    return path;
  }
  return "C:\\temp"; // Fallback
#else
  const char *tmpdir = std::getenv("TMPDIR");
  if (tmpdir != nullptr) {
    return std::string(tmpdir);
  }
  return "/tmp";
#endif
}

std::string PathUtils::get_username() {
#ifdef _WIN32
  const char *username = std::getenv("USER");
  if (username == nullptr) {
    username = std::getenv("USERNAME");
    if (username == nullptr) {
      username = "default";
    }
  }
  return std::string(username);
#else
  const char *username = std::getenv("USER");
  if (username == nullptr) {
    uid_t uid = getuid();
    struct passwd pw, *pw_result = nullptr;
    char buffer[16384];

    int result = getpwuid_r(uid, &pw, buffer, sizeof(buffer), &pw_result);
    if (result == 0 && pw_result != nullptr && pw_result->pw_name != nullptr) {
      username = pw_result->pw_name;
    } else {
      username = "default";
    }
  }
  return std::string(username);
#endif
}

std::string PathUtils::get_default_aeron_dir() {
  std::string temp_dir = get_temp_dir();
  std::string username = get_username();

#ifdef _WIN32
  return join_path(temp_dir, "aeron-" + username);
#else
  return join_path(temp_dir, "aeron-" + username);
#endif
}

bool PathUtils::create_directory(const std::string &path) {
#ifdef _WIN32
  return _mkdir(path.c_str()) == 0;
#else
  return mkdir(path.c_str(), 0755) == 0;
#endif
}

bool PathUtils::directory_exists(const std::string &path) {
#ifdef _WIN32
  return _access(path.c_str(), 0) == 0;
#else
  return access(path.c_str(), F_OK) == 0;
#endif
}

bool PathUtils::remove_directory(const std::string &path) {
#ifdef _WIN32
  return _rmdir(path.c_str()) == 0;
#else
  return rmdir(path.c_str()) == 0;
#endif
}

char PathUtils::get_file_separator() {
#ifdef _WIN32
  return '\\';
#else
  return '/';
#endif
}

std::string PathUtils::join_path_impl() { return ""; }

} // namespace util
} // namespace aeron
