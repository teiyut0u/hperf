/**
 * @file args_parser.cpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2025-09-08
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "hperf/args_parser.h"

#include <unistd.h>    // For sysconf
#include <getopt.h>

#include <cstdlib>
#include <iostream>

#include "hperf/pmu_config.h"

bool ArgsParser::parse(ProfileConfig &profile_config, int argc, char **argv) {
  const char *short_opts = "d:i:ac:p:o:h";
  const option long_opts[] = {{"duration", required_argument, nullptr, 'd'},
                              {"interval", required_argument, nullptr, 'i'},
                              {"system_wide", no_argument, nullptr, 'a'},
                              {"cpu", required_argument, nullptr, 'c'},
                              {"pid", required_argument, nullptr, 'p'},
                              {"output", required_argument, nullptr, 'o'},
                              {"detect-counters", no_argument, nullptr, 1},
                              {"optimize-event-groups", no_argument, nullptr, 2},
                              {"help", no_argument, nullptr, 'h'},
                              {nullptr, 0, nullptr, 0}};

  int opt;
  std::string cpu_list_str;
  bool a_flag = false;
  bool p_flag = false;
  bool cmd_flag = false;

  while ((opt = getopt_long(argc,
                            argv,
                            short_opts,
                            long_opts,
                            nullptr)) != -1) {
    switch (opt) {
      case 'd':
        profile_config.test_duration = std::atoi(optarg);
        break;
      case 'i':
        profile_config.switch_group_interval = std::atoi(optarg);
        break;
      case 'a':
        a_flag = true;
        break;
      case 'c':
        cpu_list_str = optarg;
        break;
      case 'p':
        p_flag = true;
        profile_config.target_pid = std::atoi(optarg);
        break;
      case 'o':
        profile_config.output_filename = optarg;
        break;
      case 1:
        profile_config.detect_counters = true;
        return true;  // if option '--detect-counters' specified, end parsing immediately
      case 2:
        profile_config.optimize_event_groups = true;
        break;
      case 'h':
        print_help(argv[0]);
        exit(0);
      default:
        print_help(argv[0]);
        exit(1);
    }
  }

  // Parse remaining arguments as command to execute
  for (int i = optind; i < argc; ++i) {
    profile_config.command_args.push_back(argv[i]);
  }
  if (!profile_config.command_args.empty()) {
    profile_config.command_args.push_back(nullptr);  // null-terminate the array
    cmd_flag = true;
  }

  // For validating the options ...
  // - Profiling model
  int flags = 0;
  if (a_flag) {
    flags++;
    profile_config.mode = ProfileMode::SYSTEM_WIDE;
  }
  if (p_flag) {
    flags++;
    profile_config.mode = ProfileMode::TRACK_PID;
  }
  if (cmd_flag) {
    flags++;
    profile_config.mode = ProfileMode::SUBPROCESS;
  }

  if (flags > 1) {
    std::cerr << "Error: Cannot use multiple measurement modes simultaneously.\n";
    return false;
  }

  if (flags == 0) {
    std::cerr << "Error: You must specify either -a (system-wide), -p <PID> (per-process), "
              << "or provide a command to execute.\n";
    return false;
  }

  if (a_flag && profile_config.test_duration <= 0) {
    std::cerr << "Error: For system-wide, test duration must be greater than 0.\n";
    return false;
  }

  if (a_flag) {
    int num_cpus = sysconf(_SC_NPROCESSORS_ONLN);  // the number of processors currently online.
    if (!cpu_list_str.empty()) {
      profile_config.cpu_id_list = parse_comma_sperated_list(cpu_list_str);
      if (profile_config.cpu_id_list.empty()) {
        std::cerr << "Error: Invalid CPU ID list (" << cpu_list_str << ").\n";
        return false;
      }
    } else {  // no specify -c option, put all online CPUs in the list
      for (int cpu = 0; cpu < num_cpus; ++cpu) {
        profile_config.cpu_id_list.push_back(cpu);
      }
    }
  }

  return true;
}

void ArgsParser::print_profile_config(const ProfileConfig &profile_config) {
  std::cout << "========= Profiling Configuration ==========\n";

  if (profile_config.test_duration > 0) {
    std::cout << "Test duration: " << profile_config.test_duration << " seconds\n";
  } else {
    std::cout << "Test duration: unlimited\n";
  }

  std::cout << "Event group switch inteval: " << profile_config.switch_group_interval << " ms\n";

  std::cout << "Mode: ";
  switch (profile_config.mode) {
    case ProfileMode::SYSTEM_WIDE:
      std::cout << "system-wide measurement";
      break;
    case ProfileMode::TRACK_PID:
      std::cout << "per-process measurement (tracking PID)";
      break;
    case ProfileMode::SUBPROCESS:
      std::cout << "per-process measurement (command-line)";
      break;
    default:
      std::cout << "UNKNOWN";
      break;
  }
  std::cout << "\n";

  std::cout << "CPU ID list: [";
  for (size_t i = 0; i < profile_config.cpu_id_list.size(); ++i) {
    if (i >= profile_config.cpu_id_list.size() - 1) {
      std::cout << profile_config.cpu_id_list[i];
    } else {
      std::cout << profile_config.cpu_id_list[i] << ", ";
    }
  }
  std::cout << "]\n";

  std::cout << "Output file name: " << profile_config.output_filename << "\n";
  std::cout << "Output file descriptor: " << (profile_config.output_file_ptr ? "set" : "null") << "\n";
  std::cout << "Target PID: " << profile_config.target_pid << "\n";

  std::cout << "Command Args: [";
  for (size_t i = 0; i < profile_config.command_args.size(); ++i) {
    if (profile_config.command_args[i] != nullptr) {
      std::cout << "\"" << profile_config.command_args[i] << "\"";
      if (i < profile_config.command_args.size() - 1 &&
          profile_config.command_args[i + 1] != nullptr) {
        std::cout << ", ";
      }
    }
  }
  std::cout << "]\n";
  std::cout << "============================================\n";
}

std::vector<int> ArgsParser::parse_comma_sperated_list(std::string cpu_id_str) {
  std::vector<int> result;
  if (cpu_id_str.empty()) return result;

  size_t pos = 0;
  while (pos < cpu_id_str.size()) {
    size_t comma_pos = cpu_id_str.find(',', pos);  // find the next ','
    std::string token = cpu_id_str.substr(pos, comma_pos - pos);
    if (token.empty()) {
      return std::vector<int>();
    }

    size_t dash_pos = token.find('-');
    if (dash_pos == std::string::npos) {  // a single CPU ID
      char *endptr = nullptr;
      int cpu = std::strtol(token.c_str(), &endptr, 10);
      if (*endptr != '\0' || cpu < 0) {
        return std::vector<int>();
      }
      result.push_back(cpu);
    } else {  // a CPU ID range
      std::string start_str = token.substr(0, dash_pos);
      std::string end_str = token.substr(dash_pos + 1);
      char *endptr1 = nullptr;
      char *endptr2 = nullptr;
      int start = std::strtol(start_str.c_str(), &endptr1, 10);
      int end = std::strtol(end_str.c_str(), &endptr2, 10);
      if (*endptr1 != '\0' || *endptr2 != '\0' || start < 0 || end < 0 || end < start) {
        return std::vector<int>();
      }
      for (int i = start; i <= end; ++i) {
        result.push_back(i);
      }
    }
    if (comma_pos == std::string::npos) break;
    pos = comma_pos + 1;
  }
  return result;
}

void ArgsParser::print_help(const char *program_name) {
  std::cout
      << "Usage: " << program_name << " [options] [command [command-args]]\n"
      << "         Efficiently collect PMU data by multiplexing hardware counters.\n"
      << "         Specify the target by -p <PID> option or giving a command.\n"
      << "         Use -a option to conduct system-wide monitoring.\n"
      << "Options:\n"
      << "  -d, --duration <seconds>    Specify the test duration in seconds.\n"
      << "  -i, --interval <ms>         Specify the test duration in ms.\n"
      << "  -a, --system-wide           System-wide measurement.\n"
      << "  -c, --target_cpu <cpu>      Only for system-wide, only monitor the specified CPUs.\n"
      << "                              Multiple CPUs can be provided as a comma-separated list.\n"
      << "  -p, --pid <PID>             Per-process measurement by specifying PID.\n"
      << "  -o, --output <file>         Print the raw data into the designated file.\n"
      << "      --detect-counters       Detect the number of programmable hardware counters on each CPU and exit.\n"
      << "      --optimize-event-groups Detect counters, and use the result to optimize default event groups.\n"
      << "  -h, --help                  Show this help message and exit.\n"
      << "\nExample:\n"
      << "  Specify a PID\n"
      << "    " << program_name << " -p 1234 -d 5 -i 100\n"
      << "  Give a command\n"
      << "    " << program_name << " -i 500 /bin/sleep 10\n"
      << "  System-wide monitor\n"
      << "    " << program_name << " -a -d 10 -i 1000\n"
      << "\nPMU Events List:\n";

  PMUConfig pmu_config;
  pmu_config.print_pmu_config();
}