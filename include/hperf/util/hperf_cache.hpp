#ifndef HPERF_CACHE_HPP
#define HPERF_CACHE_HPP

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace hperf {
namespace fs = std::filesystem;

class HperfCache {
 public:
  inline static fs::path CACHE_DIR() {
    static const fs::path path = get_cache_dir();  // 只计算一次
    return path;
  }

  inline static fs::path COUNTER_CACHE() {
    static const fs::path path = CACHE_DIR() / "counter";
    return path;
  }

  inline static bool clean(const std::string& target) {
    try {
      if (target == "all") {
        for (const auto& entry : fs::directory_iterator(CACHE_DIR())) {
          remove_target(entry);
        }
      } else {
        fs::path to_remove = fs::path(target);
        remove_target(CACHE_DIR() / to_remove);
      }
      return true;
    } catch (const fs::filesystem_error& e) {
      return false;
    }
  }

  inline static std::vector<std::string> list() {
    std::vector<std::string> res;
    if (fs::exists(CACHE_DIR())) {
      traverse_dirs_name(CACHE_DIR(), fs::directory_entry(CACHE_DIR()), res);
    }
    return res;
  }

 private:
  inline static fs::path get_cache_dir() {
    fs::path cache_dir;
    if (const char* home = std::getenv("HOME")) {
      cache_dir = fs::path(home) / ".cache" / "hperf";
    } else {
      cache_dir = fs::path("/tmp/hperf_cache");
    }
    if (!fs::exists(cache_dir)) {
      try {
        fs::create_directories(cache_dir);
      } catch (const fs::filesystem_error& e) {
        std::cerr << "Failed to create cache directory: \"" << cache_dir << "\"\n";
      }
    }
    return cache_dir;
  }

  inline static void remove_target(const fs::directory_entry& to_remove) {
    if (to_remove.is_directory()) {
      fs::remove_all(to_remove.path());
    } else {
      fs::remove(to_remove.path());
    }
  }

  inline static void remove_target(const fs::path& to_remove) {
    if (fs::is_directory(to_remove)) {
      fs::remove_all(to_remove);
    } else {
      fs::remove(to_remove);
    }
  }

  inline static void traverse_dirs_name(const fs::path& base_dir, const fs::directory_entry& current_dir, std::vector<std::string>& res) {
    try {
      for (const auto& entry : fs::directory_iterator(current_dir)) {
        if (entry.is_directory()) {
          traverse_dirs_name(base_dir, entry, res);
        } else {
          res.push_back(fs::relative(entry.path(), base_dir));
        }
      }
    } catch (const fs::filesystem_error& e) {
      std::cerr << "Failed to access " << current_dir.path().string() << std::endl;
    }
  }
};

}  // namespace hperf

#endif
