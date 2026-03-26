#include "hperf/monitor/monitor_util.hpp"

#include <sys/stat.h>

#include <array>

const fs::path MonitorUtil::DEVICES_DIR{"/sys/bus/event_source/devices"};

std::error_code MonitorUtil::read_file(std::string& result, const fs::path& file_path, unsigned int retry_times) {
  // open file
  FILE* fp = fopen(file_path.c_str(), "rb");
  // return error if failed to open
  if (!fp) {
    return std::error_code(errno, std::generic_category());
  }
  // check file state
  int fd = fileno(fp);
  struct stat st;
  if (fstat(fd, &st) != 0) {
    int saved_errno = errno;
    fclose(fp);
    return std::error_code(saved_errno, std::generic_category());
  }
  // this function read regular file
  if (!S_ISREG(st.st_mode)) {
    fclose(fp);
    if (S_ISDIR(st.st_mode)) {
      return std::make_error_code(std::errc::is_a_directory);
    }
    return std::make_error_code(std::errc::invalid_argument);
  }
  // read file
  if (retry_fread(fp, retry_times, result)) {
    fclose(fp);
    return {};
  } else {
    fclose(fp);
    return std::make_error_code(std::errc::io_error);
  }
}

std::error_code MonitorUtil::add_watchpoint_monitor_attr(const fs::path device_path, uint16_t nodeid_encode, std::string_view event_name, PerfEventAttr& attr) {
  attr.set_disabled();
  attr.set_read_format(PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING);
  // add type
  auto type_err = attr.set_type(device_path / "type");
  if (type_err) {
    // printf("Failed to add_type\n");
    return type_err;
  }
  // add event
  auto event_err = attr.add_event(device_path / "events" / event_name);
  if (event_err) {
    // printf("Failed to add_event\n");
    return event_err;
  }
  // add param
  std::array<std::pair<std::string_view, uint64_t>, 6> params{{
      {"bynodeid", 0x1},
      {"nodeid", nodeid_encode & (uint64_t(-1) << 3)},
      {"wp_dev_sel", (nodeid_encode & 0x4) >> 2},
      {"wp_chn_sel", 0x3},
      {"wp_val", 0x0},
      {"wp_mask", 0xffffffffffffffff},
  }};
  fs::path format_path = device_path / "format";
  for (const auto& param : params) {
    auto field_err = attr.add_field(format_path / param.first, param.second);
    if (field_err) {
      // printf("Failed to add_field\n");
      return field_err;
    }
  }
  return std::error_code{};
}

bool MonitorUtil::retry_fread(FILE* stream, unsigned int retry_times, std::string& result) {
  char buffer[4096];
  for (unsigned int attempt = 0; attempt <= retry_times; ++attempt) {
    if (attempt > 0) {
      // clear before retry
      clearerr(stream);
      result.clear();
    }

    size_t n;
    while ((n = fread(buffer, sizeof(char), sizeof(buffer), stream)) > 0) {
      result.append(buffer, n);
    }

    if (!ferror(stream)) {
      return true;
    }
  }
  return false;
}

std::error_code add_watchpoint_monitor_attr(const fs::path device_path, uint16_t nodeid_encode, std::string_view event_name, PerfEventAttr& attr) {
  attr.set_disabled();
  attr.set_read_format(PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING);
  // add type
  auto type_err = attr.set_type(device_path / "type");
  if (type_err) {
    // printf("Failed to add_type\n");
    return type_err;
  }
  // add event
  auto event_err = attr.add_event(device_path / "events" / event_name);
  if (event_err) {
    // printf("Failed to add_event\n");
    return event_err;
  }
  // add param
  std::array<std::pair<std::string_view, uint64_t>, 6> params{{
      {"bynodeid", 0x1},
      {"nodeid", nodeid_encode & (uint64_t(-1) << 3)},
      {"wp_dev_sel", (nodeid_encode & 0x4) >> 2},
      {"wp_chn_sel", 0x3},
      {"wp_val", 0x0},
      {"wp_mask", 0xffffffffffffffff},
  }};
  fs::path format_path = device_path / "format";
  for (const auto& param : params) {
    auto field_err = attr.add_field(format_path / param.first, param.second);
    if (field_err) {
      // printf("Failed to add_field\n");
      return field_err;
    }
  }
  return std::error_code{};
}
