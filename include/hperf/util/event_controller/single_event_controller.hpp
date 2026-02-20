#ifndef SINGLE_EVENT_CONTROLLER_HPP
#define SINGLE_EVENT_CONTROLLER_HPP

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <system_error>
#include <vector>

#include "hperf/util/hperf_result.hpp"

namespace hperf {

class SingleEventController {
 public:
  SingleEventController();
  ~SingleEventController();
  SingleEventController(SingleEventController& another) = delete;
  SingleEventController(SingleEventController&& another) noexcept;
  SingleEventController& operator=(SingleEventController& another) = delete;
  SingleEventController& operator=(SingleEventController&& another) noexcept;

  std::error_code open_event(perf_event_attr* attr_ptr, pid_t pid, int cpu, unsigned long flags);

  template <typename T>
  hperf::Result<int, std::error_code> control_event(unsigned long request, T arg);
  inline hperf::Result<int, std::error_code> enable_event() { return this->control_event(PERF_EVENT_IOC_ENABLE, 0); }
  inline hperf::Result<int, std::error_code> disable_event() { return this->control_event(PERF_EVENT_IOC_DISABLE, 0); }
  inline hperf::Result<int, std::error_code> reset_event() { return this->control_event(PERF_EVENT_IOC_RESET, 0); }
  std::error_code read_event();
  std::error_code close_event();

  // size_t size() const;
  uint64_t value() const;
  std::optional<uint64_t> time_enabled() const;
  std::optional<uint64_t> time_running() const;
  std::optional<uint64_t> id() const;
  std::optional<uint64_t> lost() const;

 private:
  uint64_t time_enabled_offset_;
  uint64_t time_running_offset_;
  uint64_t id_offset_;
  uint64_t lost_offset_;

  int fd_;
  // std::unique_ptr<perf_event_attr> attr_ptr;
  uint64_t read_format_;
  std::vector<std::byte> buffer_;
};

template <typename T>
inline hperf::Result<int, std::error_code> hperf::SingleEventController::control_event(unsigned long request, T arg) {
  if (this->fd_ == -1) {
    return hperf::make_error_result<int, std::error_code>(std::make_error_code(std::errc::bad_file_descriptor));
  }
  int result_val = ::ioctl(this->fd_, request, arg);
  if (result_val == -1) {
    return hperf::make_error_result<int, std::error_code>(errno, std::generic_category());
  } else {
    return hperf::make_result<int, std::error_code>(result_val);
  }
}

}  // namespace hperf

#endif
