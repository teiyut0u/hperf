#pragma once

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>  // For close, read, ioctl
#include <unistd.h>

#include <cstring>  // For strerror
#include <string>
#include <vector>

#include "pmu_config.h"
#include "read_buffer.h"

/**
 * @brief Control hardware counter multiplexing. It creates file descriptors (fds) using perf_event_open system call and read buffers for each event group. It is also responsible for controlling scheduling during measurement.
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
   */
  EventScheduler(PMUConfig &pmu_config, pid_t target_pid, int target_cpu);

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
   * @brief Initializes file descriptors and read format for each event group. 
   *
   * @return true On success
   * @return false On failure
   */
  bool initialize();

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
   * @brief Switch to the next event group during the measurement. 
   * It disables the current active event group, enables the next, and resets the event count of the new active event group.
   *
   * @return true On success
   * @return false On failure
   */
  bool switch_to_next_group();

  /**
   * @brief Reads data from the currently active group.
   * The caller is responsible for providing a buffer of appropriate size.
   * Use get_active_read_format_info().rf_size to determine buffer size.
   *
   * @param buffer
   * @param buffer_size
   * @return ssize_t
   */
  ssize_t read_active_group_data();

  GroupReadBuffer& get_active_group_read_buffer();
  int get_active_group_idx() const;
  bool is_initialized() const;
  int get_num_event_groups() const;
  const std::vector<PMUEvent> &get_pmu_events_in_active_group() const;

 private:
  std::vector<std::vector<int>> fds_;
  std::vector<GroupReadBuffer> read_buffers_;  // One Group Read Buffer per event group

  PMUConfig &pmu_config_;
  pid_t target_pid_;  // -1 for any CPU if target_pid_ is set, or specific CPU for system-wide
  int target_cpu_;

  int active_group_idx_;  // Current active event group index, starts from 0

  bool initialized_;  // true if it has been initialized

  /**
   * @brief Clear the already-created event file descriptors. 
   * 
   */
  void cleanup_fds();

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
   * @brief Helper function to setup a perf_event_attr structure which provides detailed configuration information for the event being created (see man perf_open_event).
   *
   * @param[out] pe The pointer of the perf_event_attr structure that has already been set up
   * @param type Event type (PERF_TYPE_RAW, PERF_TYPE_HARDWARE, PERF_TYPE_SOFTWARE, ...)
   * @param config Event encoding
   * @param is_group_leader Is this event a group leader or not
   */
  static void configure_event(struct perf_event_attr *pe,
                              uint32_t type,
                              uint64_t config,
                              bool is_group_leader);

  /**
   * @brief The wrapper for perf_event_open system call.
   *
   * It returns file descriptor of the event on success and print error message on failure.
   *
   * @param pe Configuration information for the event being created
   * @param pid Which process to monitor, -1 means all processes/threads
   * @param cpu Which CPU to monitor, -1 means any CPUs
   * @param group_fd The event file descriptor of group leader when creating group member. When creating group leader, group_fd = -1
   * @param flags See 'mam perf_event_open', usually 0
   * @return int The event file descriptor
   */
  static int perf_event_open(struct perf_event_attr *pe,
                             pid_t pid,
                             int cpu,
                             int group_fd,
                             unsigned long flags);
};
