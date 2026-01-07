#pragma once

#include <fstream>  // for std::ofstream
#include <string>   // for std::string
#include <vector>   // for std::vector

enum ProfileMode { SYSTEM_WIDE,
                   TRACK_PID,
                   SUBPROCESS };

/**
 * @brief The struct to store the profiling options parsed from the command line
 * 
 */
struct ProfileConfig {
  ProfileMode mode = SYSTEM_WIDE;

  int test_duration = -1;            // 'd': test duration
  int switch_group_interval = 1000;  // 'i': event group switching interval
  std::vector<int> cpu_id_list;      // 'c': CPU list
  pid_t target_pid = -1;             // 'p': target PID
  std::string output_filename = "";  // 'o': output file name

  std::ofstream *output_file_ptr = nullptr;  // file stream for the output file

  std::vector<char *> command_args;  // command

  bool detect_counters = false;  // 'detect-counters': detect the number of programmable counters

  bool optimize_event_groups = false;  // 'optimize-event-groups': detect the number of programmable counters, and use the result to optimize the default event groups
};
