/**
 * @file main.cpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2025-08-21
 *
 * @copyright Copyright (c) 2025
 *
 */

#include <errno.h>
#include <getopt.h>
#include <linux/wait.h>
#include <signal.h>    // For kill
#include <sys/wait.h>  // For waitpid

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <thread>

#include "hperf/args_parser.h"
#include "hperf/counter_detector.h"
#include "hperf/event_scheduler.h"
#include "hperf/pmu_config.h"
#include "hperf/reporter.h"

#define MAX_TEST_DURATION 600  // Max test duration: 600s

/**
 * @brief Get the timestamp in nanoseconds since epoch object
 *
 * @param t
 * @return uint64_t
 */
uint64_t get_timestamp_since_epoch(std::chrono::steady_clock::time_point t) {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch()).count();
}

/**
 * @brief System-wide measurement, collect performance data on all CPUs or specified CPU(s)
 *
 * @param config
 * @param reporter
 */
void system_wide_measurement(PMUConfig &pmu_config, const ProfileConfig &config, Reporter &reporter) {
  // create and initialize event groups on each CPU
  std::vector<EventScheduler> event_scheduler_list;
  for (const auto cpu : config.cpu_id_list) {
    EventScheduler event_scheduler(pmu_config, -1, cpu);
    if (!event_scheduler.initialize()) {
      std::cerr << "Fail to initialize the event scheduler on CPU " << cpu << "\n";
      return;  // stop measurement
    } else {
      event_scheduler_list.push_back(std::move(event_scheduler));
    }
  }

  // Reset all counters
  for (int i = 0; i < config.cpu_id_list.size(); i++) {
    if (!event_scheduler_list[i].reset_all_groups()) {
      std::cerr << "Fail to reset counters on CPU " << config.cpu_id_list[i] << "\n";
      return;  // stop measurement
    }
  }

  auto start = std::chrono::steady_clock::now();
  auto end = start + std::chrono::seconds(config.test_duration);

  uint64_t start_timestamp = get_timestamp_since_epoch(start);

  // Enable (the first) event group
  for (int i = 0; i < config.cpu_id_list.size(); i++) {
    if (!event_scheduler_list[i].enable_active_group()) {
      std::cerr << "Fail to reset counters on CPU " << config.cpu_id_list[i] << "\n";
      return;  // stop measurement
    }
  }

  std::cout << "System-wide: collecting data...\n";

  while (std::chrono::steady_clock::now() < end) {
    std::this_thread::sleep_for(std::chrono::milliseconds(config.switch_group_interval));

    uint64_t current_timestamp = get_timestamp_since_epoch(std::chrono::steady_clock::now());
    for (int i = 0; i < config.cpu_id_list.size(); i++) {
      if (event_scheduler_list[i].read_active_group_data() > 0) {
        const auto &buffer = event_scheduler_list[i].get_active_group_read_buffer();
        for (uint64_t j = 0; j < buffer.nr(); ++j) {
          Record record = {
              current_timestamp - start_timestamp,
              config.cpu_id_list[i],
              event_scheduler_list[i].get_active_group_idx(),
              j,
              buffer.entry(j)->value};
          reporter.process_a_record(record);
          reporter.print_a_record(record, config.output_file_ptr ? *config.output_file_ptr : std::cout);
        }
      } else {
        std::cerr << "Fail to read event counts on CPU " << config.cpu_id_list[i] << ": "
                  << strerror(errno) << "\n";
      }
    }

    // Switch to the next event group
    for (int i = 0; i < config.cpu_id_list.size(); i++) {
      if (!event_scheduler_list[i].switch_to_next_group())
        std::cerr << "Warning: Failed to properly switch event group on CPU " << config.cpu_id_list[i]
                  << std::endl;
    }
  }  // end while

  // Stop the last active group
  for (int i = 0; i < config.cpu_id_list.size(); i++) {
    if (!event_scheduler_list[i].disable_active_group()) {
      std::cerr << "Fail to stop counters on CPU " << config.cpu_id_list[i] << "\n";
    }
  }

  std::cout << "System-wide: data collection finished" << std::endl;
}

void per_process_measurement(PMUConfig &pmu_config, const ProfileConfig &config, Reporter &reporter) {
  EventScheduler event_scheduler(pmu_config, config.target_pid, -1);
  if (!event_scheduler.initialize()) {
    std::cerr << "Fail to initialize event groups for PID " << config.target_pid << "\n";
    return;  // stop measurement
  }

  // Reset all counters
  if (!event_scheduler.reset_all_groups()) {
    std::cerr << "Fail to reset counters for PID " << config.target_pid << "\n";
    return;  // stop measurement
  }

  auto start = std::chrono::steady_clock::now();

  int duration = config.test_duration > 0 ? config.test_duration : MAX_TEST_DURATION;
  auto end = start + std::chrono::seconds(duration);

  uint64_t start_timestamp = get_timestamp_since_epoch(start);

  // Enable (the first) event group for the PID
  if (!event_scheduler.enable_active_group()) {
    std::cerr << "Fail to reset counters for PID " << config.target_pid << "\n";
    return;  // stop measurement
  }

  std::cout << "Per-process (Target PID: " << config.target_pid << "): collecting data...\n";

  while (std::chrono::steady_clock::now() < end) {
    std::this_thread::sleep_for(std::chrono::milliseconds(config.switch_group_interval));

    // Check the target process
    if (config.target_pid != -1) {
      int status;
      pid_t result = waitpid(config.target_pid, &status, WNOHANG);

      if (result > 0) {  // subprocess, terminated
        std::cout << "Target process " << config.target_pid << " has terminated, stopping measurement.\n";
        break;
      } else if (result == -1 && errno == ECHILD) {  // not subprocess
        if (kill(config.target_pid, 0) == -1 && errno == ESRCH) {
          std::cout << "Target process " << config.target_pid << " no longer exists, stopping measurement.\n";
          break;
        }
        // kill return 0: still running
      }
      // result == 0: subprocess, still running
    }

    uint64_t current_timestamp = get_timestamp_since_epoch(std::chrono::steady_clock::now());

    int active_group_idx = event_scheduler.get_active_group_idx();

    if (event_scheduler.read_active_group_data() > 0) {
      const auto &buffer = event_scheduler.get_active_group_read_buffer();
      for (uint64_t i = 0; i < buffer.nr(); ++i) {
        Record record = {
            current_timestamp - start_timestamp,
            -1,
            active_group_idx,
            i,
            buffer.entry(i)->value};
        reporter.process_a_record(record);
        reporter.print_a_record(record, config.output_file_ptr ? *config.output_file_ptr : std::cout);
      }
    } else {
      std::cerr << "Fail to read event counts for PID " << config.target_pid << ": "
                << strerror(errno) << "\n";
    }

    // Switch to the next event group
    if (!event_scheduler.switch_to_next_group() && event_scheduler.get_num_event_groups() > 1) {
      std::cerr << "Warning: Failed to properly switch event group for PID " << config.target_pid
                << std::endl;
    }
  }  // end while

  // Stop the last active group
  if (!event_scheduler.disable_active_group()) {
    std::cerr << "Fail to stop counters for PID " << config.target_pid << "\n";
  }

  std::cout << "Per-process (Target PID: " << config.target_pid << "): data collection finished"
            << std::endl;
}

/**
 * @brief Execute a command and return its PID
 *
 * @param command_args Array of command arguments (null-terminated)
 * @return pid_t PID of the child process, or -1 on error
 */
pid_t execute_command(char *const command_args[]) {
  pid_t child_pid = fork();

  if (child_pid == 0) {
    // Child process: execute the command
    execvp(command_args[0], command_args);
    // If execvp returns, there was an error
    std::cerr << "Error: Failed to execute command '" << command_args[0]
              << "': " << strerror(errno) << std::endl;
    exit(1);
  } else if (child_pid > 0) {
    // Parent process: return child PID
    return child_pid;
  } else {
    // Fork failed
    std::cerr << "Error: Failed to fork process: " << strerror(errno) << std::endl;
    return -1;
  }
}

int main(int argc, char **argv) {
  PMUConfig pmu_config;
  if (!pmu_config.is_valid()) {
    std::cerr << "Error: PMU event configuration is invalid." << std::endl;
    return 1;
  }

  ProfileConfig profile_config;
  ArgsParser args_parser;

  // Step 1 Parse the command-line options into profiling config
  if (!args_parser.parse(profile_config, argc, argv)) {
    return 1;
  }

  // Detect counters?
  if (profile_config.detect_counters) {
    CounterDetector counter_detector;
    std::cout << "Detecting available programmable counters on each CPU ..." << std::endl;
    counter_detector.detect();
    counter_detector.print_result();
    return 0;
  }

  if (profile_config.optimize_event_groups) {
    CounterDetector counter_detector;
    std::cout << "Detecting available programmable counters on each CPU ..." << std::endl;
    counter_detector.detect();
    counter_detector.print_result();

    std::cout << "Adaptive Grouping: " << std::endl;
    std::cout << "Before:" << std::endl;
    pmu_config.print_event_groups_by_line();

    pmu_config.adaptive_grouping(counter_detector.get_detected_general_counter_num() - pmu_config.get_fixed_events().size());

    std::cout << "After:" << std::endl;
    pmu_config.print_event_groups_by_line();
  }

  Reporter reporter(pmu_config);

  // Step 1.1 Execute command if specified
  if (profile_config.mode == ProfileMode::SUBPROCESS) {
    std::cout << "Executing command: ";
    for (size_t i = 0; i < profile_config.command_args.size() - 1; ++i) {
      std::cout << profile_config.command_args[i] << " ";
    }
    std::cout << std::endl;

    pid_t child_pid = execute_command(profile_config.command_args.data());
    if (child_pid == -1) {
      std::cerr << "Error: Failed to execute the command." << std::endl;
      return 1;
    }

    std::cout << "Command started with PID: " << child_pid << std::endl;

    // Save the subprocess PID in the profiling config
    profile_config.target_pid = child_pid;
    // Small delay to let the process start
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // Step 1.2 For per-process measurement (given a PID or a command), check if the PID exists
  if (profile_config.target_pid != -1) {
    if (kill(profile_config.target_pid, 0) == -1) {
      if (errno == ESRCH) {  // process does not exist
        std::cerr << "Error: Process with PID " << profile_config.target_pid << " does not exist.\n";
      } else {
        std::cerr << "Error: Failed to check existence of PID " << profile_config.target_pid << ": "
                  << strerror(errno) << "\n";
      }
      return 1;
    }
    std::cout << "Monitoring process with PID: " << profile_config.target_pid << "\n";
  }

  // Step 1.3 Set output file stream, if specified
  std::ofstream output_file;
  if (!profile_config.output_filename.empty()) {
    output_file.open(profile_config.output_filename);
    if (!output_file.is_open()) {
      std::cerr << "Error: Failed to open output file: " << profile_config.output_filename << "\n";
      return 1;
    } else {
      std::cout << "Outputting data to " << profile_config.output_filename << "\n";
      output_file << "timestamp,cpu,group,event,value\n";
    }
    profile_config.output_file_ptr = &output_file;
  }

  // Step 1.4 Print Profiling config
  args_parser.print_profile_config(profile_config);

  // Step 2 Conduct measurement
  if (profile_config.mode == ProfileMode::SYSTEM_WIDE) {
    system_wide_measurement(pmu_config, profile_config, reporter);
  } else {
    per_process_measurement(pmu_config, profile_config, reporter);
  }

  // Step 3 Show performance data
  reporter.estimation();
  reporter.print_stats();
  reporter.print_metrics();

  return 0;
}
