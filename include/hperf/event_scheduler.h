#pragma once

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>  // For close, read, ioctl

#include <cstring>  // For strerror
#include <string>
#include <vector>

#include "pmu_config.h"
#include "read_buffer.h"

/**
 * @brief Control hardware counter multiplexing. It creates file descriptors (fds) using perf_event_open system call and read buffers for each event group. It is also responsible for controlling scheduling during measurement.
 *
 * Supports two scheduling modes:
 * - Kernel mode (default): All groups are enabled simultaneously; the kernel multiplexes
 *   schedulable groups. Fixed events are opened as a pinned group.
 * - User mode (legacy): Only one group is active at a time; userspace timer switches groups.
 *
 * Note that for system-wide measurement, each specified CPU has its own EventScheduler instance.
 */
class EventScheduler {
 public:
  /**
   * @brief Construct a new EventScheduler object, but not initialize it.
   *
   * @param pmu_config Reference to a PMUConfig object which contains static PMU event configuration
   * @param target_pid Process PID to be monitored, -1 for system-wide measurement
   * @param target_cpu A single CPU ID to be monitored, -1 for per-process measurement. If multiple CPUs are specified for system-wide measurement, each specified CPU has an EventGroups.
   * @param kernel_mode If true, use kernel-mode scheduling (pinned fixed events, all groups enabled simultaneously)
   */
  EventScheduler(const PMUConfig &pmu_config, pid_t target_pid, int target_cpu, bool kernel_mode = false);

  /**
   * @brief Move constructor
   *
   * @param other
   */
  EventScheduler(EventScheduler &&other) noexcept;

  /**
   * @brief Move assignment
   *
   * @param other
   * @return EventScheduler&
   */
  EventScheduler &operator=(EventScheduler &&other) noexcept;

  // Prevent copy constructor
  EventScheduler(const EventScheduler &) = delete;
  EventScheduler &operator=(const EventScheduler &) = delete;

  /**
   * @brief Destroy the EventScheduler object and free all allocated resources.
   *
   */
  ~EventScheduler();

  /**
   * @brief Initialize file descriptors and read buffers for each event group.
   *
   * In kernel mode: fds_[0] = pinned fixed events, fds_[1..N] = schedulable groups (no fixed events).
   * In user mode: fds_[i] = fixed events + schedulable events (existing behavior).
   *
   * @throws HperfError if any perf_event_open call fails. Any file descriptors
   *         already opened are automatically released by the destructor via RAII.
   */
  void initialize();

  /**
   * @brief Reset the event count of all event groups.
   * It should be called before the measurement starts.
   *
   * @return true On success
   * @return false On failure
   */
  bool reset_all_groups();

  /**
   * @brief Reset the event count of the active event group.
   * It is typically be called after the event group switching
   *
   * @return true On success
   * @return false On failure
   */
  bool reset_active_group();

  /**
   * @brief Enable the active event group.
   *
   * @return true On success
   * @return false On failure
   */
  bool enable_active_group();

  /**
   * @brief Disable the active event group.
   *
   * @return true On success
   * @return false On failure
   */
  bool disable_active_group();

  /**
   * @brief Enable all event groups simultaneously (kernel mode).
   *
   * @return true On success
   * @return false On failure
   */
  bool enable_all_groups();

  /**
   * @brief Disable all event groups (kernel mode).
   *
   * @return true On success
   * @return false On failure
   */
  bool disable_all_groups();

  /**
   * @brief Switch to the next event group during the measurement.
   * It disables the current active event group, enables the next, and resets the event count of the new active event group.
   *
   * @return true On success
   * @return false On failure
   */
  bool switch_to_next_group();

  /**
   * @brief Read counter data from the currently active event group into the
   * internal read buffer. The buffer is accessible via get_active_group_read_buffer().
   *
   * @return ssize_t Number of bytes read on success, -1 on failure.
   */
  ssize_t read_active_group_data();

  /**
   * @brief Read counter data from a specific event group into its read buffer.
   *
   * @param group_idx Index of the group to read.
   * @return ssize_t Number of bytes read on success, -1 on failure.
   */
  ssize_t read_group_data(int group_idx);

  /** @brief Return the read buffer for the currently active event group. */
  GroupReadBuffer &get_active_group_read_buffer();

  /** @brief Return the read buffer for a specific event group. */
  GroupReadBuffer &get_group_read_buffer(int group_idx);

  /** @brief Return the index of the currently active event group. */
  int get_active_group_idx() const;

  /** @brief Return true if initialize() has been called successfully. */
  bool is_initialized() const;

  /** @brief Return the total number of groups (including pinned fixed group in kernel mode). */
  int get_num_groups() const;

  /** @brief Return the number of schedulable event groups (same as PMUConfig::get_event_group_num()). */
  int get_num_event_groups() const;

  /** @brief Return the schedulable PMU events in the currently active event group. */
  const std::vector<PMUEvent> &get_pmu_events_in_active_group() const;

  /** @brief Return whether this scheduler uses kernel-mode scheduling. */
  bool is_kernel_mode() const;

 private:
  std::vector<std::vector<int>> fds_;
  std::vector<GroupReadBuffer> read_buffers_;  // One Group Read Buffer per group

  const PMUConfig &pmu_config_;
  pid_t target_pid_;  // -1 for any CPU if target_pid_ is set, or specific CPU for system-wide
  int target_cpu_;

  int active_group_idx_;  // Current active event group index, starts from 0

  bool initialized_;  // true if it has been initialized
  bool kernel_mode_;  // true if using kernel-mode scheduling

  /**
   * @brief Clear the already-created event file descriptors.
   *
   */
  void cleanup_fds();

  void initialize_kernel_mode();
  void initialize_user_mode();

  /**
   * @brief Open perf_event fds for a group of events and store them in fds_[slot].
   * @throws HperfError on perf_event_open failure.
   */
  void open_event_group_(const std::vector<PMUEvent> &events, size_t fds_slot, bool pinned);

  /** @brief Apply an ioctl request to all groups. */
  bool control_all_groups_(unsigned long request, const std::string &action_name);

  /**
   * @brief Helper function to perform ioctl actions on event groups.
   *
   * @param group_leader_fd The group leader event file descriptor
   * @param request A perf_event ioctl action (PERF_EVENT_IOC_ENABLE, PERF_EVENT_IOC_DISABLE, PERF_EVENT_IOC_RESET, ...)
   * @param action_name The name of the action for debug usage
   * @return true On success
   * @return false On failure
   */
  bool control_group(int group_leader_fd,
                     unsigned long request,
                     const std::string &action_name);

  /**
   * @brief Helper function to setup a perf_event_attr structure which provides detailed configuration information for the event being created (see man perf_event_open).
   *
   * @param[out] pe The perf_event_attr structure to configure
   * @param type Event type (PERF_TYPE_RAW, PERF_TYPE_HARDWARE, PERF_TYPE_SOFTWARE, ...)
   * @param config Event encoding
   * @param is_group_leader Is this event a group leader or not
   * @param pinned If true, set pe.pinned=1 on the group leader (for fixed events)
   */
  static void configure_event(struct perf_event_attr &pe,
                              uint32_t type,
                              uint64_t config,
                              bool is_group_leader,
                              bool pinned = false);

  /**
   * @brief The wrapper for perf_event_open system call.
   *
   * It returns file descriptor of the event on success and print error message on failure.
   *
   * @param pe Configuration information for the event being created
   * @param pid Which process to monitor, -1 means all processes/threads
   * @param cpu Which CPU to monitor, -1 means any CPUs
   * @param group_fd The event file descriptor of group leader when creating group member. When creating group leader, group_fd = -1
   * @param flags See 'man perf_event_open', usually 0
   * @return int The event file descriptor
   */
  static int perf_event_open(struct perf_event_attr *pe,
                             pid_t pid,
                             int cpu,
                             int group_fd,
                             unsigned long flags);
};
