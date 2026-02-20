#ifndef HPERF_CACHE_HPP
#define HPERF_CACHE_HPP

#include <unistd.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace hperf {
namespace fs = std::filesystem;

class HperfCache {
 public:
  inline static fs::path CACHE_DIR() {
    static const fs::path path = get_cache_dir();  // compute once
    return path;
  }

  inline static fs::path COUNTER_CACHE() {
    static const fs::path path = CACHE_DIR() / "counter";
    return path;
  }

  static bool clean(const std::string& target);

  inline static std::vector<std::string> list() {
    std::vector<std::string> res;
    if (fs::exists(CACHE_DIR())) {
      traverse_dirs_name(CACHE_DIR(), fs::directory_entry(CACHE_DIR()), res);
    }
    return res;
  }

  static std::error_code write_cache(std::string_view target, const void* write_buffer, size_t n_bytes);

  static std::error_code read_cache(std::string_view target, void* read_buffer, size_t n_bytes);

 private:
  static fs::path get_cache_dir();

  static void remove_target(const fs::directory_entry& to_remove);

  static void remove_target(const fs::path& to_remove);

  static void traverse_dirs_name(const fs::path& base_dir, const fs::directory_entry& current_dir, std::vector<std::string>& res);
};

}  // namespace hperf

#endif
