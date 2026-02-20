#include "hperf/util/event_controller/group_event_controller.hpp"

#include <linux/perf_event.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <memory>
#include <optional>
#include <system_error>
#include <utility>

#include "hperf/util/hperf_result.hpp"
#include "hperf/util/hperf_util.hpp"

// hperf::Result<hperf::GroupEventController, std::error_code>
// hperf::GroupEventController::create(pid_t target_pid, int target_cpu, unsigned long flags, uint64_t read_format) {
//   // TODO: check it
//   auto leader_attr_ptr = hperf::make_empty_attr();
//   leader_attr_ptr->type = PERF_TYPE_SOFTWARE;
//   leader_attr_ptr->size = sizeof(struct perf_event_attr);
//   leader_attr_ptr->config = PERF_COUNT_SW_DUMMY;
//   leader_attr_ptr->read_format = read_format;
//   leader_attr_ptr->disabled = 1;
//
//   int leader_fd = perf_event_open(leader_attr_ptr.get(), target_pid, target_cpu, -1, flags);
//   if (leader_fd == -1) {
//     return hperf::make_error_result<hperf::GroupEventController, std::error_code>(
//         errno, std::generic_category());
//   } else {
//     return hperf::make_result<hperf::GroupEventController, std::error_code>(
//         std::move(
//             hperf::GroupEventController(target_pid, target_cpu, flags, read_format, leader_fd)));
//   }
// }

hperf::GroupEventController::GroupEventController(pid_t target_pid, int target_cpu, unsigned long flags, uint64_t read_format) {
  this->target_pid_ = target_pid;
  this->target_cpu_ = target_cpu;
  this->flags_ = flags & ~(PERF_FLAG_FD_NO_GROUP | PERF_FLAG_FD_OUTPUT);
  this->read_format_ = read_format | PERF_FORMAT_GROUP;

  // determine header size and offset
  this->header_size_ = this->time_enabled_offset_ = this->time_running_offset_ = sizeof(uint64_t);
  if (this->read_format_ & PERF_FORMAT_TOTAL_TIME_ENABLED) {
    this->time_running_offset_ += sizeof(uint64_t);
    this->header_size_ += sizeof(uint64_t);
  }
  if (this->read_format_ & PERF_FORMAT_TOTAL_TIME_RUNNING) {
    this->header_size_ += sizeof(uint64_t);
  }

  // determine entry size and offset
  this->entry_size_ = this->id_offset_ = this->lost_offset_ = sizeof(uint64_t);
  if (this->read_format_ & PERF_FORMAT_ID) {
    this->lost_offset_ += sizeof(uint64_t);
    this->entry_size_ += sizeof(uint64_t);
  }
  if (this->read_format_ & PERF_FORMAT_LOST) {
    this->entry_size_ += sizeof(uint64_t);
  }
}

hperf::Result<size_t, std::error_code> hperf::GroupEventController::open_event(perf_event_attr* attr_ptr, unsigned long flags) {
  uint64_t saved_disabled = attr_ptr->disabled,
           saved_read_format = attr_ptr->read_format;
  if (this->fds_.empty()) {
    attr_ptr->disabled = 1;
    attr_ptr->read_format = this->read_format_;
  } else {
    attr_ptr->disabled = 0;
    attr_ptr->read_format = 0;
  }

  int fd = perf_event_open(attr_ptr, this->target_pid_, this->target_cpu_, this->fds_.front(), flags);
  attr_ptr->disabled = saved_disabled;
  attr_ptr->read_format = saved_read_format;

  if (fd == -1) {
    return hperf::make_error_result<size_t, std::error_code>(
        errno, std::generic_category());
  } else {
    this->fds_.push_back(fd);
    this->buffer_.resize(this->header_size() + this->events_count() * this->entry_size());
    return hperf::make_result<size_t, std::error_code>(this->fds_.size() - 1);
  }
}

std::error_code hperf::GroupEventController::read_group() {
  if (read(this->fds_.front(), this->buffer_.data(), this->buffer_size()) == -1) {
    return std::error_code{errno, std::generic_category()};
  } else {
    return std::error_code{};
  }
}

std::vector<std::pair<size_t, std::error_code>> hperf::GroupEventController::close_group() {
  std::vector<std::pair<size_t, std::error_code>> result;
  for (size_t i = 0; i < this->fds_.size(); ++i) {
    if (this->fds_[i] == -1) {
      continue;
    }
    if (close(this->fds_[i]) == -1) {
      result.push_back({i, std::error_code{errno, std::generic_category()}});
    }
  }
  return result;
}

uint64_t hperf::GroupEventController::nr() const {
  return *reinterpret_cast<const uint64_t*>(this->buffer_.data());
}

std::optional<uint64_t> hperf::GroupEventController::time_enabled() const {
  if (this->read_format_ & PERF_FORMAT_TOTAL_TIME_ENABLED) {
    return std::optional<uint64_t>{*reinterpret_cast<const uint64_t*>(this->buffer_.data() + this->time_enabled_offset_)};
  } else {
    return std::nullopt;
  }
}

std::optional<uint64_t> hperf::GroupEventController::time_running() const {
  if (this->read_format_ & PERF_FORMAT_TOTAL_TIME_RUNNING) {
    return std::optional<uint64_t>{*reinterpret_cast<const uint64_t*>(this->buffer_.data() + this->time_running_offset_)};
  } else {
    return std::nullopt;
  }
}

std::vector<uint64_t> hperf::GroupEventController::value() const {
  std::vector<uint64_t> result;
  for (size_t i = 0; i < this->fds_.size(); ++i) {
    result.push_back(*reinterpret_cast<const uint64_t*>(this->buffer_.data() + this->header_size_ + i * this->entry_size_));
  }
  return result;
}

std::optional<std::vector<uint64_t>> hperf::GroupEventController::id() const {
  std::vector<uint64_t> result;
  for (size_t i = 0; i < this->fds_.size(); ++i) {
    result.push_back(*reinterpret_cast<const uint64_t*>(this->buffer_.data() + this->header_size_ + i * this->entry_size_ + this->id_offset_));
  }
  return result;
}

std::optional<std::vector<uint64_t>> hperf::GroupEventController::lost() const {
  std::vector<uint64_t> result;
  for (size_t i = 0; i < this->fds_.size(); ++i) {
    result.push_back(*reinterpret_cast<const uint64_t*>(this->buffer_.data() + this->header_size_ + i * this->entry_size_ + this->lost_offset_));
  }
  return result;
}

uint64_t hperf::GroupEventController::value(size_t event_idx) const {
  return *reinterpret_cast<const uint64_t*>(this->buffer_.data() + this->header_size_ + event_idx * this->entry_size_);
}
std::optional<uint64_t> hperf::GroupEventController::id(size_t event_idx) const {
  return *reinterpret_cast<const uint64_t*>(this->buffer_.data() + this->header_size_ + event_idx * this->entry_size_ + this->id_offset_);
}

std::optional<uint64_t> hperf::GroupEventController::lost(size_t event_idx) const {
  return *reinterpret_cast<const uint64_t*>(this->buffer_.data() + this->header_size_ + event_idx * this->entry_size_ + this->lost_offset_);
}
