#pragma once

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <system_error>
#include <vector>

#include "hperf/monitor/event_controller_interface.h"

class SingleEventController : public EventControllerInterface {
 public:
  SingleEventController();
  ~SingleEventController();
  SingleEventController(const SingleEventController& another) = delete;
  SingleEventController(SingleEventController&& another) noexcept;
  SingleEventController& operator=(const SingleEventController& another) = delete;
  SingleEventController& operator=(SingleEventController&& another) noexcept;

  std::error_code open_event(perf_event_attr* attr_ptr, pid_t pid, int cpu, unsigned long flags);

  std::error_code control(unsigned long request, void* arg) const override;

  std::error_code read() override;
  std::error_code close() override;

  size_t size() const override { return 1; }
  uint64_t value() const;
  std::vector<uint64_t> all_value() const override;
  std::optional<uint64_t> time_enabled() const override;
  std::optional<uint64_t> time_running() const override;
  std::optional<uint64_t> id() const;
  std::optional<std::vector<uint64_t>> all_id() const override;
  std::optional<uint64_t> lost() const;
  std::optional<std::vector<uint64_t>> all_lost() const override;

  uint64_t get_read_format() const override { return read_format_; }

 private:
  uint64_t time_enabled_offset_ = 0;
  uint64_t time_running_offset_ = 0;
  uint64_t id_offset_ = 0;
  uint64_t lost_offset_ = 0;

  int fd_ = -1;
  uint64_t read_format_ = 0;
  std::vector<std::byte> buffer_;
};

inline std::error_code SingleEventController::control(unsigned long request, void* arg) const {
  if (fd_ == -1) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }
  if (::ioctl(fd_, request, arg) == -1) {
    return std::error_code{errno, std::generic_category()};
  } else {
    return std::error_code{};
  }
}
