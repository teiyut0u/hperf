#ifndef HPERF_UTIL_HPP
#define HPERF_UTIL_HPP

#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <charconv>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "hperf/util/hperf_error.hpp"
#include "hperf/util/hperf_result.hpp"

namespace hperf {

namespace fs = std::filesystem;

const fs::path DEVICES_DIR{"/sys/bus/event_source/devices"};

template <typename T>
std::errc string2integer(std::string_view str_view, T& value) {
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

inline int perf_event_open(struct perf_event_attr* attr, pid_t pid, int cpu, int group_fd, unsigned long flags) {
  return syscall(__NR_perf_event_open, attr, pid, cpu, group_fd, flags);
}

std::unique_ptr<struct perf_event_attr> make_empty_attr();
std::unique_ptr<struct perf_event_attr> make_attr();

hperf::Result<std::string, std::error_code> read_file(const fs::path& file_path, unsigned int retry_times = 0);

std::error_code add_type(const fs::path& type_path, struct perf_event_attr& attr);

std::error_code add_event(const fs::path& event_path, struct perf_event_attr& attr);

std::error_code add_field(const fs::path& field_path, uint64_t field_val, struct perf_event_attr& attr);

std::vector<std::pair<std::string_view, std::string_view>> parse_param_views(
    std::string::const_iterator begin_it,
    std::string::const_iterator end_it);

hperf::HperfError parse_event_from(const std::string& event_str, struct perf_event_attr& attr);

}  // namespace hperf

#endif
