#include "hperf/monitor/monitor_util.hpp"

#include <sys/stat.h>

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
