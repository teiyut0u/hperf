#ifndef GROUP_EVENT_CONTROLLER_HPP
#define GROUP_EVENT_CONTROLLER_HPP

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <optional>
#include <system_error>
#include <utility>
#include <vector>

#include "hperf/util/hperf_result.hpp"

namespace hperf {

class GroupEventController {
 public:
  GroupEventController(pid_t target_pid, int target_cpu, unsigned long flags, uint64_t read_format);

  // static hperf::Result<hperf::GroupEventController, std::error_code>
  // create(pid_t target_pid, int target_cpu, unsigned long flags, uint64_t read_format);

  // return event index or error
  inline hperf::Result<size_t, std::error_code> open_event(perf_event_attr* attr_ptr) { return open_event(attr_ptr, this->flags_); }
  hperf::Result<size_t, std::error_code> open_event(perf_event_attr* attr_ptr, unsigned long flags);
  // group
  template <typename T>
  hperf::Result<int, std::error_code> control_group(unsigned long request, T arg);
  inline hperf::Result<int, std::error_code> enable_group() { return this->control_group(PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP); }
  inline hperf::Result<int, std::error_code> disable_group() { return this->control_group(PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP); }
  inline hperf::Result<int, std::error_code> reset_group() { return this->control_group(PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP); }

  std::error_code read_group();
  std::vector<std::pair<size_t, std::error_code>> close_group();

  uint64_t nr() const;
  std::optional<uint64_t> time_enabled() const;
  std::optional<uint64_t> time_running() const;
  std::vector<uint64_t> value() const;
  std::optional<std::vector<uint64_t>> id() const;
  std::optional<std::vector<uint64_t>> lost() const;

  // return how many events
  inline uint64_t events_count() const { return this->fds_.size(); }
  inline uint64_t header_size() const { return this->header_size_; }
  inline uint64_t entry_size() const { return this->entry_size_; }
  inline uint64_t buffer_size() const { return this->buffer_.size(); }
  inline unsigned long get_flags() const { return this->flags_; }
  inline uint64_t get_read_format() const { return this->read_format_; }

  // event
  // template <typename T>
  // inline hperf::Result<int, std::error_code> control_event(size_t fd_idx, unsigned long request, T arg) {
  // }
  // std::error_code read_event(size_t idx);
  // std::error_code close_event(size_t event_idx);
  uint64_t value(size_t event_idx) const;
  std::optional<uint64_t> id(size_t event_idx) const;
  std::optional<uint64_t> lost(size_t event_idx) const;

 private:
  uint64_t read_format_;
  pid_t target_pid_;
  int target_cpu_;
  unsigned long flags_;
  size_t header_size_;
  size_t entry_size_;

  uint64_t time_enabled_offset_;
  uint64_t time_running_offset_;
  uint64_t id_offset_;
  uint64_t lost_offset_;

  std::vector<int> fds_;
  std::vector<std::byte> buffer_;
};

template <typename T>
hperf::Result<int, std::error_code> GroupEventController::control_group(unsigned long request, T arg) {
  if (this->fds_.empty()) {
    return make_error_result<int, std::error_code>(
        std::make_error_code(std::errc::bad_file_descriptor));
  }
  int result = ioctl(this->fds_.front(), request, arg);
  if (result == -1) {
    return make_error_result<int, std::error_code>(errno, std::generic_category());
  } else {
    return make_result<int, std::error_code>(result);
  }
}

}  // namespace hperf

#endif
