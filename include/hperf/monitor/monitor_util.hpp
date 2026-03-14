#ifndef HPERF_UTIL_HPP
#define HPERF_UTIL_HPP

#include <sys/syscall.h>
#include <unistd.h>

#include <charconv>
#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

class MonitorUtil {
 public:
  static const fs::path DEVICES_DIR;

  static std::error_code read_file(std::string& result, const fs::path& file_path, unsigned int retry_times = 0);

  inline static int perf_event_open(struct perf_event_attr* attr, pid_t pid, int cpu, int group_fd, unsigned long flags) {
    return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
  }

  template <typename T>
  static std::errc string2integer(std::string_view str_view, T& value) {
    if (str_view.empty()) {
      return std::errc::invalid_argument;
    }
    const char* begin_parse;
    int base;
    if (str_view[0] == '0') {
      if (str_view.size() == 1) {
        value = 0;
        return std::errc();
      }
      if (str_view[1] == 'x' || str_view[1] == 'X') {
        begin_parse = str_view.data() + 2;
        base = 16;
      } else if (str_view[1] == 'b' || str_view[1] == 'B') {
        begin_parse = str_view.data() + 2;
        base = 2;
      } else {
        begin_parse = str_view.data() + 1;
        base = 8;
      }
    } else {
      begin_parse = str_view.data();
      base = 10;
    }

    auto [ptr, ec] = std::from_chars(begin_parse, str_view.data() + str_view.size(), value, base);
    if (ec != std::errc()) {
      return ec;
    }

    return std::errc();
  }

 private:
  static bool retry_fread(FILE* stream, unsigned int retry_times, std::string& result);
};

#endif
