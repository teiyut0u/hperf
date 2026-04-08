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

#include <getopt.h>
#include <linux/limits.h>  // For PATH_MAX
#include <sys/wait.h>      // For waitpid
#include <unistd.h>        // For readlink

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
#include "hperf/hperf_error.h"
#include "hperf/pmu_config.h"
#include "hperf/reporter.h"

#ifndef __ANDROID__
#include "hperf/monitor/cmn_bandwidth_monitor.h"
#endif

constexpr int kMaxTestDuration = 600;  // Max test duration: 600s

static volatile sig_atomic_t g_interrupted = 0;

static void sigint_handler(int) {
  g_interrupted = 1;
}

/**
 * @brief Return the path to the hidden config file in the same directory as the executable.
 *
 * Resolves /proc/self/exe and appends "/.hperf.toml".
 * Returns an empty string on error.
 */
static std::string get_config_path() {
  char exe_path[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
  if (len == -1) return "";
  exe_path[len] = '\0';
  std::string path(exe_path);
  size_t slash = path.rfind('/');
  std::string dir = (slash != std::string::npos) ? path.substr(0, slash) : ".";
  return dir + "/.hperf.toml";
}

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
 * @brief Check if the watched process has exited. Returns true if we should stop.
 */
static bool check_process_exited(pid_t watch_pid) {
  if (watch_pid == -1) return false;
  int status;
  pid_t result = waitpid(watch_pid, &status, WNOHANG);
  if (result > 0) {  // our child, has terminated
    std::cout << "Target process " << watch_pid << " has terminated, stopping measurement.\n";
    return true;
  } else if (result == -1 && errno == ECHILD) {  // not our child
    if (kill(watch_pid, 0) == -1 && errno == ESRCH) {
      std::cout << "Target process " << watch_pid << " no longer exists, stopping measurement.\n";
      return true;
    }
  }
  return false;
}

/**
 * @brief User-mode sampling loop: switches one event group at a time (legacy behavior).
 */
static void run_user_mode_loop(std::vector<EventScheduler> &schedulers,
                               const std::vector<int> &cpu_ids,
                               uint64_t start_timestamp,
                               std::chrono::steady_clock::time_point end,
                               const ProfileConfig &config,
                               Reporter &reporter,
                               pid_t watch_pid) {
  while (!g_interrupted && std::chrono::steady_clock::now() < end) {
    std::this_thread::sleep_for(std::chrono::milliseconds(config.switch_group_interval));

    if (check_process_exited(watch_pid)) break;

    uint64_t current_timestamp = get_timestamp_since_epoch(std::chrono::steady_clock::now());

    // Read data from every scheduler
    for (size_t i = 0; i < schedulers.size(); ++i) {
      int active_group_idx = schedulers[i].get_active_group_idx();
      if (schedulers[i].read_active_group_data() > 0) {
        const auto &buffer = schedulers[i].get_active_group_read_buffer();
        for (uint64_t j = 0; j < buffer.nr(); ++j) {
          auto entry = buffer.entry(j);
          if (!entry) continue;
          Record record = {current_timestamp - start_timestamp, cpu_ids[i], active_group_idx, j,
                           entry->value};
          reporter.process_a_record(record);
          reporter.print_a_record(record, *config.output_stream);
        }
      } else {
        if (cpu_ids[i] >= 0)
          std::cerr << "Fail to read event counts on CPU " << cpu_ids[i] << ": " << strerror(errno) << "\n";
        else
          std::cerr << "Fail to read event counts for PID " << watch_pid << ": " << strerror(errno) << "\n";
      }
    }

    // Advance all schedulers to the next event group
    for (size_t i = 0; i < schedulers.size(); ++i) {
      if (!schedulers[i].switch_to_next_group() && schedulers[i].get_num_event_groups() > 1) {
        if (cpu_ids[i] >= 0)
          std::cerr << "Warning: Failed to properly switch event group on CPU " << cpu_ids[i] << "\n";
        else
          std::cerr << "Warning: Failed to properly switch event group for PID " << watch_pid << "\n";
      }
    }
  }
  if (g_interrupted) {
    std::cout << "\nInterrupted. Computing results from collected data...\n";
  }
}

/**
 * @brief Kernel-mode sampling loop: all groups enabled simultaneously, kernel handles multiplexing.
 *        Periodically reads all groups from all schedulers and feeds deltas to the reporter.
 */
static void run_kernel_mode_loop(std::vector<EventScheduler> &schedulers,
                                 const std::vector<int> &cpu_ids,
                                 uint64_t start_timestamp,
                                 std::chrono::steady_clock::time_point end,
                                 const ProfileConfig &config,
                                 Reporter &reporter,
                                 pid_t watch_pid) {
  // prev_values[scheduler_idx][group_idx][event_idx] for delta computation
  // Cache num_groups per scheduler — these are constant after initialize().
  std::vector<int> num_groups_per_sched(schedulers.size());
  std::vector<std::vector<std::vector<uint64_t>>> prev_values(schedulers.size());
  for (size_t s = 0; s < schedulers.size(); ++s) {
    num_groups_per_sched[s] = schedulers[s].get_num_groups();
    prev_values[s].resize(num_groups_per_sched[s]);
    for (int g = 0; g < num_groups_per_sched[s]; ++g) {
      size_t num_events = schedulers[s].get_group_read_buffer(g).event_num();
      prev_values[s][g].resize(num_events, 0);
    }
  }

  // Process a read buffer: compute deltas vs prev_values, feed records to reporter.
  // Returns true if any events were processed.
  auto process_group_deltas = [&](size_t s, int g, uint64_t rel_timestamp, bool print) {
    const auto &buffer = schedulers[s].get_group_read_buffer(g);
    uint64_t nr = buffer.nr();
    for (uint64_t j = 0; j < nr && j < prev_values[s][g].size(); ++j) {
      auto entry = buffer.entry(j);
      if (!entry) continue;
      uint64_t delta = entry->value - prev_values[s][g][j];
      prev_values[s][g][j] = entry->value;
      if (delta == 0 && !print) continue;
      Record record = {rel_timestamp, cpu_ids[s], g, j, delta};
      reporter.process_a_record(record);
      if (print) reporter.print_a_record(record, *config.output_stream);
    }
  };

  while (!g_interrupted && std::chrono::steady_clock::now() < end) {
    std::this_thread::sleep_for(std::chrono::milliseconds(config.switch_group_interval));

    if (check_process_exited(watch_pid)) break;

    uint64_t rel_timestamp = get_timestamp_since_epoch(std::chrono::steady_clock::now()) - start_timestamp;

    for (size_t s = 0; s < schedulers.size(); ++s) {
      for (int g = 0; g < num_groups_per_sched[s]; ++g) {
        if (schedulers[s].read_group_data(g) > 0) {
          process_group_deltas(s, g, rel_timestamp, /*print=*/true);
        } else {
          if (cpu_ids[s] >= 0)
            std::cerr << "Fail to read event counts on CPU " << cpu_ids[s] << " group " << g << ": " << strerror(errno) << "\n";
          else
            std::cerr << "Fail to read event counts for PID " << watch_pid << " group " << g << ": " << strerror(errno) << "\n";
        }
      }
    }
  }

  // Final read: collect remaining deltas and kernel time_enabled/time_running
  uint64_t final_rel_timestamp = get_timestamp_since_epoch(std::chrono::steady_clock::now()) - start_timestamp;
  for (size_t s = 0; s < schedulers.size(); ++s) {
    for (int g = 0; g < num_groups_per_sched[s]; ++g) {
      if (schedulers[s].read_group_data(g) > 0) {
        process_group_deltas(s, g, final_rel_timestamp, /*print=*/false);
        const auto &buffer = schedulers[s].get_group_read_buffer(g);
        reporter.add_kernel_time(g, buffer.time_enabled(), buffer.time_running());
      }
    }
  }

  if (g_interrupted) {
    std::cout << "\nInterrupted. Computing results from collected data...\n";
  }
}

/**
 * @brief System-wide measurement, collect performance data on all CPUs or specified CPU(s)
 */
void system_wide_measurement(PMUConfig &pmu_config, const ProfileConfig &config, Reporter &reporter) {
  bool kernel_mode = !config.user_mode_sched;

  std::vector<EventScheduler> schedulers;
  for (const auto cpu : config.cpu_id_list) {
    EventScheduler es(pmu_config, -1, cpu, kernel_mode);
    es.initialize();  // throws HperfError on failure; RAII cleans up fds
    schedulers.push_back(std::move(es));
  }

  for (size_t i = 0; i < schedulers.size(); ++i) {
    if (!schedulers[i].reset_all_groups()) {
      std::cerr << "Fail to reset counters on CPU " << config.cpu_id_list[i] << "\n";
      return;
    }
  }

  auto start = std::chrono::steady_clock::now();
  auto end = start + std::chrono::seconds(config.test_duration);
  uint64_t start_timestamp = get_timestamp_since_epoch(start);

  if (kernel_mode) {
    for (size_t i = 0; i < schedulers.size(); ++i) {
      if (!schedulers[i].enable_all_groups()) {
        std::cerr << "Fail to enable counters on CPU " << config.cpu_id_list[i] << "\n";
        return;
      }
    }

    std::cout << "System-wide (kernel scheduling): collecting data...\n";
    run_kernel_mode_loop(schedulers, config.cpu_id_list, start_timestamp, end, config, reporter, -1);

    uint64_t total_ns = get_timestamp_since_epoch(std::chrono::steady_clock::now()) - start_timestamp;
    reporter.set_total_time(total_ns);

    for (size_t i = 0; i < schedulers.size(); ++i) {
      if (!schedulers[i].disable_all_groups())
        std::cerr << "Fail to stop counters on CPU " << config.cpu_id_list[i] << "\n";
    }
  } else {
    for (size_t i = 0; i < schedulers.size(); ++i) {
      if (!schedulers[i].enable_active_group()) {
        std::cerr << "Fail to enable counters on CPU " << config.cpu_id_list[i] << "\n";
        return;
      }
    }

    std::cout << "System-wide (user-mode scheduling): collecting data...\n";
    run_user_mode_loop(schedulers, config.cpu_id_list, start_timestamp, end, config, reporter, -1);

    for (size_t i = 0; i < schedulers.size(); ++i) {
      if (!schedulers[i].disable_active_group())
        std::cerr << "Fail to stop counters on CPU " << config.cpu_id_list[i] << "\n";
    }
  }

  std::cout << "System-wide: data collection finished\n";
}

void per_process_measurement(PMUConfig &pmu_config, const ProfileConfig &config, Reporter &reporter) {
  bool kernel_mode = !config.user_mode_sched;

  std::vector<EventScheduler> schedulers;
  schedulers.emplace_back(pmu_config, config.target_pid, -1, kernel_mode);
  schedulers[0].initialize();  // throws HperfError on failure; RAII cleans up fds

  if (!schedulers[0].reset_all_groups()) {
    std::cerr << "Fail to reset counters for PID " << config.target_pid << "\n";
    return;
  }

  auto start = std::chrono::steady_clock::now();
  int duration = config.test_duration > 0 ? config.test_duration : kMaxTestDuration;
  auto end = start + std::chrono::seconds(duration);
  uint64_t start_timestamp = get_timestamp_since_epoch(start);

  if (kernel_mode) {
    if (!schedulers[0].enable_all_groups()) {
      std::cerr << "Fail to enable counters for PID " << config.target_pid << "\n";
      return;
    }

    std::cout << "Per-process (Target PID: " << config.target_pid << ", kernel scheduling): collecting data...\n";
    run_kernel_mode_loop(schedulers, {-1}, start_timestamp, end, config, reporter, config.target_pid);

    uint64_t total_ns = get_timestamp_since_epoch(std::chrono::steady_clock::now()) - start_timestamp;
    reporter.set_total_time(total_ns);

    if (!schedulers[0].disable_all_groups())
      std::cerr << "Fail to stop counters for PID " << config.target_pid << "\n";
  } else {
    if (!schedulers[0].enable_active_group()) {
      std::cerr << "Fail to enable counters for PID " << config.target_pid << "\n";
      return;
    }

    std::cout << "Per-process (Target PID: " << config.target_pid << ", user-mode scheduling): collecting data...\n";
    run_user_mode_loop(schedulers, {-1}, start_timestamp, end, config, reporter, config.target_pid);

    if (!schedulers[0].disable_active_group())
      std::cerr << "Fail to stop counters for PID " << config.target_pid << "\n";
  }

  std::cout << "Per-process (Target PID: " << config.target_pid << "): data collection finished\n";
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
              << "': " << strerror(errno) << '\n';
    exit(1);
  } else if (child_pid > 0) {
    // Parent process: return child PID
    return child_pid;
  } else {
    // Fork failed
    std::cerr << "Error: Failed to fork process: " << strerror(errno) << '\n';
    return -1;
  }
}

int main(int argc, char **argv) {
  ProfileConfig profile_config;
  ArgsParser args_parser;

  // Step 1 Parse the command-line options into profiling config
  if (!args_parser.parse(profile_config, argc, argv)) {
    return 2;
  }
  if (profile_config.help_requested) {
    args_parser.print_help(argv[0]);
    return 0;
  }

#ifndef __ANDROID__
  // Monitor mode: CMN bandwidth monitoring (Linux only, no PMU config needed)
  if (profile_config.monitor_target != MonitorTarget::NO_MONITOR_TARGET) {
    return cmn_bandwidth_monitor(profile_config) ? 0 : 1;
  }
#endif

  // All remaining modes require PMU config
  PMUConfig pmu_config;
  std::string config_path = get_config_path();
  if (config_path.empty()) {
    std::cerr << "Error: Failed to load PMU configuration.\n"
              << "       Make sure .hperf.toml exists in the same directory as this binary.\n";
    return 1;
  }
  try {
    pmu_config.load_from_file(config_path);
  } catch (const HperfError &e) {
    std::cerr << "Error: Failed to load PMU configuration: " << e.what() << "\n"
              << "       Make sure .hperf.toml exists in the same directory as this binary.\n";
    return 1;
  }

  // --list-events: print PMU event list and exit
  if (profile_config.list_events) {
    pmu_config.print_pmu_config();
    return 0;
  }

  // Detect counters?
  if (profile_config.detect_counters) {
    CounterDetector counter_detector;
    std::cout << "Detecting available programmable counters on each CPU ..." << '\n';
    counter_detector.detect();
    counter_detector.print_result();
    return 0;
  }

  if (profile_config.optimize_event_groups) {
    CounterDetector counter_detector;
    std::cout << "Detecting available programmable counters on each CPU ..." << '\n';
    counter_detector.detect();
    counter_detector.print_result();

    std::cout << "Adaptive Grouping: " << '\n';
    std::cout << "Before:" << '\n';
    pmu_config.print_event_groups_by_line();

    pmu_config.adaptive_grouping(counter_detector.get_detected_general_counter_num() - pmu_config.get_fixed_events().size());

    std::cout << "After:" << '\n';
    pmu_config.print_event_groups_by_line();
  }

  // For system-wide mode: default to all online CPUs if -c was not specified
  if (profile_config.mode == ProfileMode::SYSTEM_WIDE) {
    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (profile_config.cpu_id_list.empty()) {
      for (int cpu = 0; cpu < num_cpus; ++cpu) {
        profile_config.cpu_id_list.push_back(cpu);
      }
    } else {
      for (int cpu_id : profile_config.cpu_id_list) {
        if (cpu_id < 0 || cpu_id >= num_cpus) {
          std::cerr << "Error: CPU " << cpu_id << " is invalid (online CPUs: 0-" << num_cpus - 1 << ").\n";
          return 2;
        }
      }
    }
  }

  bool kernel_mode = !profile_config.user_mode_sched;
  Reporter reporter(pmu_config, kernel_mode);

  // Step 1.1 Execute command if specified
  if (profile_config.mode == ProfileMode::SUBPROCESS) {
    std::cout << "Executing command: ";
    for (size_t i = 0; i < profile_config.command_args.size() - 1; ++i) {
      std::cout << profile_config.command_args[i] << " ";
    }
    std::cout << '\n';

    pid_t child_pid = execute_command(profile_config.command_args.data());
    if (child_pid == -1) {
      std::cerr << "Error: Failed to execute the command." << '\n';
      return 1;
    }

    std::cout << "Command started with PID: " << child_pid << '\n';

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

  // Step 1.3 Set output stream: file if specified, otherwise stdout
  std::ofstream output_file;
  profile_config.output_stream = &std::cout;
  if (!profile_config.output_filename.empty()) {
    output_file.open(profile_config.output_filename);
    if (!output_file.is_open()) {
      std::cerr << "Error: Failed to open output file: " << profile_config.output_filename << "\n";
      return 1;
    }
    std::cout << "Outputting data to " << profile_config.output_filename << "\n";
    output_file << "timestamp,cpu,group,event,value\n";
    profile_config.output_stream = &output_file;
  }

  // Step 1.4 Print Profiling config
  args_parser.print_profile_config(profile_config);

  // Step 2 Conduct measurement (Ctrl+C stops early and proceeds to report)
  struct sigaction sa {};
  sa.sa_handler = sigint_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;  // no SA_RESTART: let sleep_for return early on signal
  sigaction(SIGINT, &sa, nullptr);
  try {
    if (profile_config.mode == ProfileMode::SYSTEM_WIDE) {
      system_wide_measurement(pmu_config, profile_config, reporter);
    } else {
      per_process_measurement(pmu_config, profile_config, reporter);
    }
  } catch (const HperfError &e) {
    std::cerr << "Error: " << e.what() << '\n';
    return 1;
  }

  // Step 3 Show performance data
  reporter.estimation();
  reporter.print_stats();
  reporter.print_metrics();

  return 0;
}
