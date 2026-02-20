#include "hperf/util/hperf_cache.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <system_error>

namespace fs = std::filesystem;

bool hperf::HperfCache::clean(const std::string& target) {
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

std::error_code hperf::HperfCache::write_cache(std::string_view target, const void* write_buffer, size_t n_bytes) {
  if (target.back() == '/' || target.empty()) {
    return std::make_error_code(std::errc::invalid_argument);
  }
  auto target_path = CACHE_DIR() / target;
  std::error_code ec;
  fs::create_directories(target_path.parent_path(), ec);
  if (ec) {
    return ec;
  }
  FILE* write_cache_fp = fopen(target_path.c_str(), "wb");
  if (!write_cache_fp) {
    return std::error_code{errno, std::generic_category()};
  }
  if (fwrite(write_buffer, 1, n_bytes, write_cache_fp) != n_bytes) {
    auto is_err = ferror(write_cache_fp);
    auto saved_errno = errno;
    fclose(write_cache_fp);
    if (is_err) {
      return std::error_code{saved_errno, std::generic_category()};
    } else {
      // eof is seen error this time
      return std::make_error_code(std::errc::io_error);
    }
  }
  if (fclose(write_cache_fp)) {
    return std::error_code{errno, std::generic_category()};
  }
  return std::error_code{};
}

std::error_code hperf::HperfCache::read_cache(std::string_view target, void* read_buffer, size_t n_bytes) {
  if (target.back() == '/' || target.empty()) {
    return std::make_error_code(std::errc::invalid_argument);
  }
  auto target_path = CACHE_DIR() / target;
  FILE* read_cache_fp = fopen(target_path.c_str(), "rb");
  if (!read_cache_fp) {
    return std::error_code{errno, std::generic_category()};
  }
  if (fread(read_buffer, 1, n_bytes, read_cache_fp) != n_bytes) {
    auto is_err = ferror(read_cache_fp);
    auto saved_errno = errno;
    fclose(read_cache_fp);
    if (is_err) {
      return std::error_code{saved_errno, std::generic_category()};
    } else {
      // eof is seen error this time
      return std::make_error_code(std::errc::io_error);
    }
  }
  // ignore protentional err here because read is successful any way
  fclose(read_cache_fp);
  return std::error_code{};
}

fs::path hperf::HperfCache::get_cache_dir() {
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

void hperf::HperfCache::remove_target(const fs::directory_entry& to_remove) {
  if (to_remove.is_directory()) {
    fs::remove_all(to_remove.path());
  } else {
    fs::remove(to_remove.path());
  }
}

void hperf::HperfCache::remove_target(const fs::path& to_remove) {
  if (fs::is_directory(to_remove)) {
    fs::remove_all(to_remove);
  } else {
    fs::remove(to_remove);
  }
}

void hperf::HperfCache::traverse_dirs_name(const fs::path& base_dir, const fs::directory_entry& current_dir, std::vector<std::string>& res) {
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
