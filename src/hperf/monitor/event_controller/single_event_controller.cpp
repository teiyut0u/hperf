#include "hperf/monitor/event_controller/single_event_controller.hpp"

#include <linux/perf_event.h>
#include <unistd.h>

#include <optional>
#include <system_error>

#include "hperf/monitor/monitor_util.hpp"

SingleEventController::SingleEventController() {
  this->time_enabled_offset_ = 8;
  this->fd_ = -1;
}

SingleEventController::~SingleEventController() {
  if (this->close()) {
    // TODO: log it
  }
}

SingleEventController::SingleEventController(SingleEventController&& another) noexcept {
  *this = std::move(another);
}

SingleEventController& SingleEventController::operator=(SingleEventController&& another) noexcept {
  this->time_enabled_offset_ = another.time_enabled_offset_;
  this->time_running_offset_ = another.time_running_offset_;
  this->id_offset_ = another.id_offset_;
  this->lost_offset_ = another.lost_offset_;

  if (this->close()) {
    // TODO: log it
  }
  this->fd_ = another.fd_;
  another.fd_ = -1;
  this->read_format_ = another.read_format_;
  this->buffer_ = std::move(another.buffer_);
  return *this;
}

std::error_code SingleEventController::open_event(perf_event_attr* attr_ptr, pid_t pid, int cpu, unsigned long flags) {
  // open event
  // close the old then open the new
  // return -1 and set errno when failed
  if (this->close()) {
    // TODO: log it
  }
  uint64_t saved_read_format = attr_ptr->read_format;
  attr_ptr->read_format &= ~PERF_FORMAT_GROUP;
  if ((this->fd_ = MonitorUtil::perf_event_open(attr_ptr, pid, cpu, -1, flags)) == -1) {
    attr_ptr->read_format = saved_read_format;
    return std::error_code{errno, std::generic_category()};
  };

  // reconfig the buffer
  this->read_format_ = attr_ptr->read_format;
  size_t buffer_size_ = 8;
  this->time_running_offset_ = this->id_offset_ = this->lost_offset_ = 8;
  if (this->read_format_ & PERF_FORMAT_TOTAL_TIME_ENABLED) {
    this->time_running_offset_ += 8;
    this->id_offset_ += 8;
    this->lost_offset_ += 8;
    buffer_size_ += 8;
  }
  if (this->read_format_ & PERF_FORMAT_TOTAL_TIME_RUNNING) {
    this->id_offset_ += 8;
    this->lost_offset_ += 8;
    buffer_size_ += 8;
  }
  if (this->read_format_ & PERF_FORMAT_ID) {
    this->lost_offset_ += 8;
    buffer_size_ += 8;
  }
  if (this->read_format_ & PERF_FORMAT_LOST) {
    buffer_size_ += 8;
  }
  this->buffer_.resize(buffer_size_);

  attr_ptr->read_format = saved_read_format;
  return std::error_code{};
}

std::error_code SingleEventController::read() {
  if (::read(this->fd_, this->buffer_.data(), this->buffer_.size()) == -1) {
    return std::error_code{errno, std::generic_category()};
  } else {
    return std::error_code{};
  }
}

std::error_code SingleEventController::close() {
  if (this->fd_ == -1) {
    return std::error_code{};
  }
  int old_fd = this->fd_;
  this->fd_ = -1;
  if (::close(old_fd) == -1) {
    return std::error_code{errno, std::generic_category()};
  } else {
    return std::error_code{};
  }
}

uint64_t SingleEventController::value() const {
  return *reinterpret_cast<const uint64_t*>(this->buffer_.data());
}
std::vector<uint64_t> SingleEventController::all_value() const {
  return {*reinterpret_cast<const uint64_t*>(this->buffer_.data())};
}

std::optional<uint64_t> SingleEventController::time_enabled() const {
  if (this->read_format_ & PERF_FORMAT_TOTAL_TIME_ENABLED) {
    return *reinterpret_cast<const uint64_t*>(this->buffer_.data() + this->time_enabled_offset_);
  } else {
    return std::nullopt;
  }
}

std::optional<uint64_t> SingleEventController::time_running() const {
  if (this->read_format_ & PERF_FORMAT_TOTAL_TIME_RUNNING) {
    return *reinterpret_cast<const uint64_t*>(this->buffer_.data() + this->time_running_offset_);
  } else {
    return std::nullopt;
  }
}

std::optional<uint64_t> SingleEventController::id() const {
  if (this->read_format_ & PERF_FORMAT_ID) {
    return *reinterpret_cast<const uint64_t*>(this->buffer_.data() + this->id_offset_);
  } else {
    return std::nullopt;
  }
}

std::optional<std::vector<uint64_t>> SingleEventController::all_id() const {
  if (this->read_format_ & PERF_FORMAT_ID) {
    return std::vector<uint64_t>{*reinterpret_cast<const uint64_t*>(this->buffer_.data() + this->id_offset_)};
  } else {
    return std::nullopt;
  }
}

std::optional<uint64_t> SingleEventController::lost() const {
  if (this->read_format_ & PERF_FORMAT_LOST) {
    return *reinterpret_cast<const uint64_t*>(this->buffer_.data() + this->lost_offset_);
  } else {
    return std::nullopt;
  }
}

std::optional<std::vector<uint64_t>> SingleEventController::all_lost() const {
  if (this->read_format_ & PERF_FORMAT_LOST) {
    return std::vector<uint64_t>{*reinterpret_cast<const uint64_t*>(this->buffer_.data() + this->lost_offset_)};
  } else {
    return std::nullopt;
  }
}
