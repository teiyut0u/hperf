#ifndef HPERF_CACHE_HPP
#define HPERF_CACHE_HPP

#include <filesystem>
#include <memory>
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

  inline static bool create_cache_dir() {
    if (!fs::exists(CACHE_DIR())) {
      try {
        fs::create_directories(CACHE_DIR());
      } catch (const fs::filesystem_error& e) {
        return false;
      }
    }
    return true;
  }

  inline static bool clean(const std::string& target) {
    try {
      if (target == "all") {
        fs::remove_all(CACHE_DIR());
      } else {
        fs::remove(CACHE_DIR() / target);
      }
      return true;
    } catch (const fs::filesystem_error& e) {
      return false;
    }
  }

  inline static std::unique_ptr<std::vector<std::string>> list() {
    auto res = std::make_unique<std::vector<std::string>>();
    if (fs::exists(CACHE_DIR())) {
      for (const auto& entry : fs::directory_iterator(CACHE_DIR())) {
        res->push_back(entry.path().filename().string());
      }
    }
    return res;
  }

 private:
  inline static fs::path get_cache_dir() {
    if (const char* home = std::getenv("HOME")) {
      return fs::path(home) / ".cache" / "hperf";
    }
    return fs::path("/tmp/hperf_cache");
  }
};

}  // namespace hperf

#endif
