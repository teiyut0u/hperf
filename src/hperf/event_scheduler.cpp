#include "hperf/event_scheduler.h"

#include <linux/perf_event.h>

#include <cstddef>
#include <iostream>
#include <vector>

#include "hperf/hperf_error.h"
#include "hperf/pmu_event.h"

EventScheduler::EventScheduler(const PMUConfig &pmu_config,
                               pid_t target_pid,
                               int target_cpu,
                               bool kernel_mode)
    : fds_(),
      pmu_config_(pmu_config),
      target_pid_(target_pid),
      target_cpu_(target_cpu),
      active_group_idx_(0),
      initialized_(false),
      kernel_mode_(kernel_mode) {
  if (kernel_mode_) {
    // Kernel mode: group 0 = fixed events, groups 1..N = schedulable event groups
    size_t schedulable_group_num = pmu_config_.get_event_group_num();
    read_buffers_.reserve(1 + schedulable_group_num);
    // Group 0: fixed events only
    read_buffers_.emplace_back(pmu_config_.get_fixed_events().size());
    // Groups 1..N: schedulable events only (no fixed events)
    for (size_t i = 0; i < schedulable_group_num; i++) {
      read_buffers_.emplace_back(pmu_config_.get_event_group_by_idx(i).size());
    }
  } else {
    // User mode: each group = fixed events + schedulable events
    size_t group_num = pmu_config_.get_event_group_num();
    read_buffers_.reserve(group_num);
    for (size_t i = 0; i < group_num; i++) {
      read_buffers_.emplace_back(pmu_config_.get_fixed_events().size() + pmu_config_.get_event_group_by_idx(i).size());
    }
  }
}

EventScheduler::EventScheduler(EventScheduler &&other) noexcept
    : fds_(std::move(other.fds_)),
      read_buffers_(std::move(other.read_buffers_)),
      pmu_config_(other.pmu_config_),
      target_pid_(other.target_pid_),
      target_cpu_(other.target_cpu_),
      active_group_idx_(other.active_group_idx_),
      initialized_(other.initialized_),
      kernel_mode_(other.kernel_mode_) {
  other.initialized_ = false;
}

EventScheduler &EventScheduler::operator=(EventScheduler &&other) noexcept {
  if (this != &other) {
    cleanup_fds();

    fds_ = std::move(other.fds_);
    read_buffers_ = std::move(other.read_buffers_);
    // pmu_config_ is a reference and cannot be rebound; both objects refer to the same PMUConfig
    target_pid_ = other.target_pid_;
    target_cpu_ = other.target_cpu_;
    active_group_idx_ = other.active_group_idx_;
    initialized_ = other.initialized_;
    kernel_mode_ = other.kernel_mode_;
  }
  other.initialized_ = false;
  return *this;
}

EventScheduler::~EventScheduler() {
  cleanup_fds();
}

void EventScheduler::cleanup_fds() {
  for (const auto &group_fds : fds_) {
    for (int fd : group_fds) {
      if (fd != -1) {
        close(fd);
      }
    }
  }
  fds_.clear();
}

void EventScheduler::initialize() {
  if (kernel_mode_) {
    initialize_kernel_mode();
  } else {
    initialize_user_mode();
  }
}

void EventScheduler::open_event_group_(const std::vector<PMUEvent> &events, size_t fds_slot, bool pinned) {
  int group_leader_fd = -1;
  bool is_first = true;
  for (const auto &pmu_event : events) {
    struct perf_event_attr pe = {};
    configure_event(pe, PERF_TYPE_RAW, pmu_event.encoding, is_first, pinned);

    int fd = perf_event_open(&pe, target_pid_, target_cpu_, group_leader_fd, 0);
    if (fd == -1) {
      throw HperfError("Failed to open event '" + pmu_event.name +
                       "' (PID: " + std::to_string(target_pid_) +
                       ", CPU: " + std::to_string(target_cpu_) + ")");
    }
    fds_[fds_slot].push_back(fd);
    if (is_first) {
      group_leader_fd = fd;
      is_first = false;
    }
  }
}

void EventScheduler::initialize_kernel_mode() {
  const auto schedulable_group_num = pmu_config_.get_event_group_num();
  fds_.resize(1 + schedulable_group_num);

  // Group 0: pinned fixed events
  open_event_group_(pmu_config_.get_fixed_events(), 0, /*pinned=*/true);

  // Groups 1..N: non-pinned schedulable event groups
  for (size_t i = 0; i < schedulable_group_num; ++i) {
    open_event_group_(pmu_config_.get_event_group_by_idx(i), 1 + i, /*pinned=*/false);
  }

  initialized_ = true;
  active_group_idx_ = 0;
}

void EventScheduler::initialize_user_mode() {
  const auto event_group_num = pmu_config_.get_event_group_num();
  fds_.resize(event_group_num);

  // Each group = fixed events + schedulable events
  for (size_t i = 0; i < event_group_num; ++i) {
    std::vector<PMUEvent> combined;
    combined.insert(combined.end(), pmu_config_.get_fixed_events().begin(), pmu_config_.get_fixed_events().end());
    combined.insert(combined.end(), pmu_config_.get_event_group_by_idx(i).begin(), pmu_config_.get_event_group_by_idx(i).end());
    open_event_group_(combined, i, /*pinned=*/false);
  }

  initialized_ = true;
  active_group_idx_ = 0;
}

bool EventScheduler::control_group(int group_leader_fd, unsigned long request,
                                   const std::string &action_name) {
  if (group_leader_fd == -1) {
    std::cerr << "Cannot " << action_name
              << " group: not initialized or invalid leader FD." << '\n';
    return false;
  }
  if (ioctl(group_leader_fd, request, PERF_IOC_FLAG_GROUP) == -1) {
    std::cerr << "Failed to " << action_name
              << " event group (FD: " << group_leader_fd
              << ", PID: " << target_pid_ << ", CPU: " << target_cpu_
              << "): " << strerror(errno) << '\n';
    return false;
  }
  return true;
}

bool EventScheduler::control_all_groups_(unsigned long request, const std::string &action_name) {
  if (!initialized_ || fds_.empty()) return false;
  for (const auto &group_fds : fds_) {
    if (group_fds.empty()) return false;
    if (!control_group(group_fds[0], request, action_name))
      return false;
  }
  return true;
}

bool EventScheduler::reset_all_groups() {
  return control_all_groups_(PERF_EVENT_IOC_RESET, "reset all");
}

bool EventScheduler::reset_active_group() {
  if (!initialized_ || fds_.empty() || fds_[active_group_idx_].empty())
    return false;
  return control_group(fds_[active_group_idx_][0], PERF_EVENT_IOC_RESET, "reset active");
}

bool EventScheduler::enable_active_group() {
  if (!initialized_ || fds_.empty() || fds_[active_group_idx_].empty())
    return false;
  return control_group(fds_[active_group_idx_][0], PERF_EVENT_IOC_ENABLE, "enable active");
}

bool EventScheduler::disable_active_group() {
  if (!initialized_ || fds_.empty() || fds_[active_group_idx_].empty())
    return false;
  return control_group(fds_[active_group_idx_][0], PERF_EVENT_IOC_DISABLE, "disable active");
}

bool EventScheduler::enable_all_groups() {
  return control_all_groups_(PERF_EVENT_IOC_ENABLE, "enable all");
}

bool EventScheduler::disable_all_groups() {
  return control_all_groups_(PERF_EVENT_IOC_DISABLE, "disable all");
}

bool EventScheduler::switch_to_next_group() {
  if (!initialized_ || fds_.empty() ||
      get_num_event_groups() <= 1) {  // No switch if 0 or 1 group
    if (get_num_event_groups() == 1 && !fds_[0].empty()) {
      // If only one group, just reset and ensure it's enabled
      if (!reset_active_group()) return false;
      return enable_active_group();
    }
    return false;
  }

  if (!disable_active_group()) {
    // Log error, but proceed to try and enable the next one
    std::cerr << "Warning: Failed to stop current group, but attempting to switch." << '\n';
  }

  // Change the active event group
  active_group_idx_ = (active_group_idx_ + 1) % get_num_event_groups();

  if (!reset_active_group()) return false;  // Reset the new active group
  return enable_active_group();             // Enable the new active group
}

ssize_t EventScheduler::read_active_group_data() {
  return read_group_data(active_group_idx_);
}

ssize_t EventScheduler::read_group_data(int group_idx) {
  if (!initialized_ || group_idx < 0 || static_cast<size_t>(group_idx) >= fds_.size()) {
    return -1;
  }
  int leader_fd = fds_[group_idx][0];
  if (leader_fd == -1) return -1;

  GroupReadBuffer &buffer = read_buffers_[group_idx];

  ssize_t bytes_read = read(leader_fd, buffer.data(), buffer.size());

  if (bytes_read == -1) {
    std::cerr << "Failed to read data for event group " << group_idx
              << " (FD: " << leader_fd << ", PID: " << target_pid_
              << ", CPU: " << target_cpu_ << "): " << strerror(errno)
              << '\n';
  } else if (static_cast<size_t>(bytes_read) != buffer.size()) {
    std::cerr << "Warning: Read " << bytes_read << " bytes, expected "
              << buffer.size() << " for event group "
              << group_idx << '\n';
  }
  return bytes_read;
}

GroupReadBuffer &EventScheduler::get_active_group_read_buffer() {
  return read_buffers_[active_group_idx_];
}

GroupReadBuffer &EventScheduler::get_group_read_buffer(int group_idx) {
  return read_buffers_[group_idx];
}

int EventScheduler::get_active_group_idx() const { return active_group_idx_; }

bool EventScheduler::is_initialized() const { return initialized_; }

int EventScheduler::get_num_groups() const {
  if (!initialized_) return 0;
  return static_cast<int>(fds_.size());
}

int EventScheduler::get_num_event_groups() const {
  if (!initialized_) return 0;
  if (kernel_mode_) {
    // In kernel mode, fds_[0] is the pinned fixed group; schedulable groups are fds_[1..N]
    return static_cast<int>(fds_.size()) - 1;
  }
  return static_cast<int>(fds_.size());
}

const std::vector<PMUEvent> &EventScheduler::get_pmu_events_in_active_group() const {
  if (!initialized_) {
    static const std::vector<PMUEvent> empty_events;  // Safe static empty vector
    std::cerr << "Error: Requesting events for invalid or uninitialized group."
              << '\n';
    return empty_events;
  }
  return pmu_config_.get_event_group_by_idx(active_group_idx_);
}

bool EventScheduler::is_kernel_mode() const { return kernel_mode_; }

void EventScheduler::configure_event(struct perf_event_attr &pe, uint32_t type,
                                     uint64_t config, bool is_group_leader,
                                     bool pinned) {
  pe = {};
  pe.type = type;
  pe.size = sizeof(struct perf_event_attr);
  pe.config = config;
  if (is_group_leader) {
    pe.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_ID | PERF_FORMAT_GROUP;
    // PERF_FORMAT_GROUP  Allows all counter values in an event group to be read
    // with one read. PERF_FORMAT_ID  Adds a 64-bit unique value that
    // corresponds to the event group.
    pe.disabled = 1;
    // When creating an event group, typically the group leader is initialized
    // with disabled set to 1 and any child events are initialized with disabled
    // set to 0. Despite disabled being 0, the child events will not start until
    // the group leader is enabled.
    if (pinned) pe.pinned = 1;
  } else {
    pe.disabled = 0;
  }
  // pe.inherit = 1;
  // [[NOTE]] The inherit bit specifies that this counter should count events of child tasks as well as the task specified.
  // Inherit does not work for some combinations of read_format values, such as PERF_FORMAT_GROUP.
}

int EventScheduler::perf_event_open(struct perf_event_attr *pe, pid_t pid, int cpu,
                                    int group_fd, unsigned long flags) {
  int fd = syscall(__NR_perf_event_open, pe, pid, cpu, group_fd, flags);
  if (fd == -1) {
    std::cerr << "perf_event_open failed for event 0x" << std::hex << pe->config
              << std::dec << " on CPU " << cpu
              << "(-1 represents any CPU): " << strerror(errno) << "\n";
  }
  return fd;
}
