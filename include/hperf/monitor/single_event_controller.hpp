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

class SingleEventController {
 public:
  SingleEventController();
  ~SingleEventController();
  SingleEventController(const SingleEventController& another) = delete;
  SingleEventController(SingleEventController&& another) noexcept;
  SingleEventController& operator=(const SingleEventController& another) = delete;
  SingleEventController& operator=(SingleEventController&& another) noexcept;

  std::error_code open_event(perf_event_attr* attr_ptr, pid_t pid, int cpu, unsigned long flags);

  template <typename T>
  std::error_code control_event(unsigned long request, T arg) const;
  inline std::error_code enable_event() const { return this->control_event(PERF_EVENT_IOC_ENABLE, 0); }
  inline std::error_code disable_event() const { return this->control_event(PERF_EVENT_IOC_DISABLE, 0); }
  inline std::error_code reset_event() const { return this->control_event(PERF_EVENT_IOC_RESET, 0); }
  std::error_code read_event();
  std::error_code close_event();

  // size_t size() const;
  uint64_t value() const;
  std::optional<uint64_t> time_enabled() const;
  std::optional<uint64_t> time_running() const;
  std::optional<uint64_t> id() const;
  std::optional<uint64_t> lost() const;

  uint64_t get_read_format() const { return read_format_; }

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
inline std::error_code SingleEventController::control_event(unsigned long request, T arg) const {
  if (this->fd_ == -1) {
    return std::make_error_code(std::errc::bad_file_descriptor);
  }
  if (::ioctl(this->fd_, request, arg) == -1) {
    return std::error_code{errno, std::generic_category()};
  } else {
    return std::error_code{};
  }
}

#endif
