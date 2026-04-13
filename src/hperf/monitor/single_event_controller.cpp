#include "hperf/monitor/single_event_controller.h"

#include <linux/perf_event.h>
#include <unistd.h>

#include <cstring>
#include <optional>
#include <system_error>

#include "hperf/monitor/monitor_util.h"

constexpr size_t kFieldSize = sizeof(uint64_t);

SingleEventController::SingleEventController() {
  time_enabled_offset_ = kFieldSize;
  fd_ = -1;
}

SingleEventController::~SingleEventController() {
  if (close()) {
    // TODO: log it
  }
}

SingleEventController::SingleEventController(SingleEventController&& another) noexcept {
  *this = std::move(another);
}

SingleEventController& SingleEventController::operator=(SingleEventController&& another) noexcept {
  time_enabled_offset_ = another.time_enabled_offset_;
  time_running_offset_ = another.time_running_offset_;
  id_offset_ = another.id_offset_;
  lost_offset_ = another.lost_offset_;

  if (close()) {
    // TODO: log it
  }
  fd_ = another.fd_;
  another.fd_ = -1;
  read_format_ = another.read_format_;
  buffer_ = std::move(another.buffer_);
  return *this;
}

std::error_code SingleEventController::open_event(perf_event_attr* attr_ptr, pid_t pid, int cpu, unsigned long flags) {
  if (close()) {
    // TODO: log it
  }
  uint64_t saved_read_format = attr_ptr->read_format;
  attr_ptr->read_format &= ~PERF_FORMAT_GROUP;
  if ((fd_ = MonitorUtil::perf_event_open(attr_ptr, pid, cpu, -1, flags)) == -1) {
    attr_ptr->read_format = saved_read_format;
    return std::error_code{errno, std::generic_category()};
  };

  // reconfig the buffer
  read_format_ = attr_ptr->read_format;
  size_t buffer_size = kFieldSize;
  time_running_offset_ = id_offset_ = lost_offset_ = kFieldSize;
  if (read_format_ & PERF_FORMAT_TOTAL_TIME_ENABLED) {
    time_running_offset_ += kFieldSize;
    id_offset_ += kFieldSize;
    lost_offset_ += kFieldSize;
    buffer_size += kFieldSize;
  }
  if (read_format_ & PERF_FORMAT_TOTAL_TIME_RUNNING) {
    id_offset_ += kFieldSize;
    lost_offset_ += kFieldSize;
    buffer_size += kFieldSize;
  }
  if (read_format_ & PERF_FORMAT_ID) {
    lost_offset_ += kFieldSize;
    buffer_size += kFieldSize;
  }
  if (read_format_ & PERF_FORMAT_LOST) {
    buffer_size += kFieldSize;
  }
  buffer_.resize(buffer_size);

  attr_ptr->read_format = saved_read_format;
  return std::error_code{};
}

std::error_code SingleEventController::read() {
  if (::read(fd_, buffer_.data(), buffer_.size()) == -1) {
    return std::error_code{errno, std::generic_category()};
  } else {
    return std::error_code{};
  }
}

std::error_code SingleEventController::close() {
  if (fd_ == -1) {
    return std::error_code{};
  }
  int old_fd = fd_;
  fd_ = -1;
  if (::close(old_fd) == -1) {
    return std::error_code{errno, std::generic_category()};
  } else {
    return std::error_code{};
  }
}

static uint64_t read_u64(const std::byte* ptr) {
  uint64_t val;
  std::memcpy(&val, ptr, sizeof(val));
  return val;
}

uint64_t SingleEventController::value() const {
  return read_u64(buffer_.data());
}
std::vector<uint64_t> SingleEventController::all_value() const {
  return {read_u64(buffer_.data())};
}

std::optional<uint64_t> SingleEventController::time_enabled() const {
  if (read_format_ & PERF_FORMAT_TOTAL_TIME_ENABLED) {
    return read_u64(buffer_.data() + time_enabled_offset_);
  } else {
    return std::nullopt;
  }
}

std::optional<uint64_t> SingleEventController::time_running() const {
  if (read_format_ & PERF_FORMAT_TOTAL_TIME_RUNNING) {
    return read_u64(buffer_.data() + time_running_offset_);
  } else {
    return std::nullopt;
  }
}

std::optional<uint64_t> SingleEventController::id() const {
  if (read_format_ & PERF_FORMAT_ID) {
    return read_u64(buffer_.data() + id_offset_);
  } else {
    return std::nullopt;
  }
}

std::optional<std::vector<uint64_t>> SingleEventController::all_id() const {
  if (read_format_ & PERF_FORMAT_ID) {
    return std::vector<uint64_t>{read_u64(buffer_.data() + id_offset_)};
  } else {
    return std::nullopt;
  }
}

std::optional<uint64_t> SingleEventController::lost() const {
  if (read_format_ & PERF_FORMAT_LOST) {
    return read_u64(buffer_.data() + lost_offset_);
  } else {
    return std::nullopt;
  }
}

std::optional<std::vector<uint64_t>> SingleEventController::all_lost() const {
  if (read_format_ & PERF_FORMAT_LOST) {
    return std::vector<uint64_t>{read_u64(buffer_.data() + lost_offset_)};
  } else {
    return std::nullopt;
  }
}
