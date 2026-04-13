#pragma once

#include <ostream>  // for std::ostream
#include <string>   // for std::string
#include <vector>   // for std::vector

enum class ProfileMode { SYSTEM_WIDE,
                         TRACK_PID,
                         SUBPROCESS };

enum class MonitorTarget {
  NO_MONITOR_TARGET,
  ARM_CMN_MEM_BW_UP,
  ARM_CMN_MEM_BW_DOWN,
  ARM_CMN_MEM_BW_ALL
};

/**
 * @brief The struct to store the profiling options parsed from the command line
 *
 */
struct ProfileConfig {
  ProfileMode mode = ProfileMode::SYSTEM_WIDE;

  int test_duration = -1;            // 'd': test duration
  int switch_group_interval = 1000;  // 'i': print interval (kernel sched) / group switch interval (user sched), ms
  std::vector<int> cpu_id_list;      // 'c': CPU list
  pid_t target_pid = -1;             // 'p': target PID
  std::string output_filename = "";  // 'o': output file name

  std::ostream *output_stream = nullptr;  // write target: set to &std::cout or &output_file in main()

  std::vector<char *> command_args;  // command

  bool detect_counters = false;  // 'detect-counters': detect the number of programmable counters

  bool optimize_event_groups = false;  // 'optimize-event-groups': detect the number of programmable counters, and use the result to optimize the default event groups

  bool user_mode_sched = false;  // --user-mode-sched: use userspace event group scheduling (legacy)

  bool list_events = false;  // --list-events: print PMU event list and exit

  bool help_requested = false;  // set by -h; main() prints help and returns 0

  // CMN bandwidth monitor options (Linux only)
  MonitorTarget monitor_target = MonitorTarget::NO_MONITOR_TARGET;
  std::string mc_position_file = "";
};
