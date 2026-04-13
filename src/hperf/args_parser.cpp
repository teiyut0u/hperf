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

#include <getopt.h>

#include <climits>
#include <cstdlib>
#include <cstring>
#include <iostream>

// Long-only option IDs, starting above ASCII range to avoid collisions with short options
enum LongOnlyOpt {
  OPT_DETECT_COUNTERS = 256,
  OPT_OPTIMIZE_EVENT_GROUPS,
  OPT_MONITOR,
  OPT_CMN_MC_POS,
  OPT_USER_MODE_SCHED,
  OPT_LIST_EVENTS,
};

bool ArgsParser::parse(ProfileConfig &profile_config, int argc, char **argv) {
  const char *short_opts = "d:i:ac:p:o:h";
  const option long_opts[] = {{"duration", required_argument, nullptr, 'd'},
                              {"interval", required_argument, nullptr, 'i'},
                              {"system_wide", no_argument, nullptr, 'a'},
                              {"cpu", required_argument, nullptr, 'c'},
                              {"pid", required_argument, nullptr, 'p'},
                              {"output", required_argument, nullptr, 'o'},
                              {"detect-counters", no_argument, nullptr, OPT_DETECT_COUNTERS},
                              {"optimize-event-groups", no_argument, nullptr, OPT_OPTIMIZE_EVENT_GROUPS},
                              {"monitor", required_argument, nullptr, OPT_MONITOR},
                              {"cmn-mc-pos", required_argument, nullptr, OPT_CMN_MC_POS},
                              {"user-mode-sched", no_argument, nullptr, OPT_USER_MODE_SCHED},
                              {"list-events", no_argument, nullptr, OPT_LIST_EVENTS},
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
      case 'd': {
        try {
          size_t pos;
          int v = std::stoi(optarg, &pos);
          if (pos != std::strlen(optarg) || v <= 0) throw std::invalid_argument("");
          profile_config.test_duration = v;
        } catch (...) {
          std::cerr << "Error: Invalid duration '" << optarg << "', must be a positive integer.\n";
          return false;
        }
        break;
      }
      case 'i': {
        try {
          size_t pos;
          int v = std::stoi(optarg, &pos);
          if (pos != std::strlen(optarg) || v <= 0) throw std::invalid_argument("");
          profile_config.switch_group_interval = v;
        } catch (...) {
          std::cerr << "Error: Invalid interval '" << optarg << "', must be a positive integer (ms).\n";
          return false;
        }
        break;
      }
      case 'a':
        a_flag = true;
        break;
      case 'c':
        cpu_list_str = optarg;
        break;
      case 'p': {
        try {
          size_t pos;
          int v = std::stoi(optarg, &pos);
          if (pos != std::strlen(optarg) || v <= 0) throw std::invalid_argument("");
          profile_config.target_pid = v;
          p_flag = true;
        } catch (...) {
          std::cerr << "Error: Invalid PID '" << optarg << "', must be a positive integer.\n";
          return false;
        }
        break;
      }
      case 'o':
        profile_config.output_filename = optarg;
        break;
      case OPT_DETECT_COUNTERS:
        profile_config.detect_counters = true;
        return true;  // if option '--detect-counters' specified, end parsing immediately
      case OPT_OPTIMIZE_EVENT_GROUPS:
        profile_config.optimize_event_groups = true;
        break;
      case OPT_MONITOR: {
#ifdef __ANDROID__
        std::cerr << "Error: --monitor is not supported on Android.\n";
        return false;
#else
        std::string target(optarg);
        if (target == "arm_cmn_mem_bw_all") {
          profile_config.monitor_target = MonitorTarget::ARM_CMN_MEM_BW_ALL;
        } else if (target == "arm_cmn_mem_bw_up") {
          profile_config.monitor_target = MonitorTarget::ARM_CMN_MEM_BW_UP;
        } else if (target == "arm_cmn_mem_bw_down") {
          profile_config.monitor_target = MonitorTarget::ARM_CMN_MEM_BW_DOWN;
        } else {
          std::cerr << "Error: Unknown monitor target '" << target << "'.\n"
                    << "       Supported targets: arm_cmn_mem_bw_all, arm_cmn_mem_bw_up, arm_cmn_mem_bw_down\n";
          return false;
        }
        break;
#endif
      }
      case OPT_CMN_MC_POS:
#ifdef __ANDROID__
        std::cerr << "Error: --cmn-mc-pos is not supported on Android.\n";
        return false;
#else
        profile_config.mc_position_file = optarg;
        break;
#endif
      case OPT_USER_MODE_SCHED:
        profile_config.user_mode_sched = true;
        break;
      case OPT_LIST_EVENTS:
        profile_config.list_events = true;
        return true;
      case 'h':
        profile_config.help_requested = true;
        return true;
      default:
        std::cerr << "Error: Unknown option.\n";
        print_help(argv[0]);
        return false;
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

  // Monitor mode validation
  if (profile_config.monitor_target != MonitorTarget::NO_MONITOR_TARGET) {
    if (a_flag || p_flag || cmd_flag) {
      std::cerr << "Error: --monitor cannot be used with -a, -p, or a command.\n";
      return false;
    }
    if (profile_config.mc_position_file.empty()) {
      std::cerr << "Error: --cmn-mc-pos <file> is required when using --monitor.\n"
                << "       Run the MC position detection script first to generate this file.\n";
      return false;
    }
    return true;  // monitor mode needs no further validation
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

  if (a_flag && !cpu_list_str.empty()) {
    profile_config.cpu_id_list = parse_comma_sperated_list(cpu_list_str);
    if (profile_config.cpu_id_list.empty()) {
      std::cerr << "Error: Invalid CPU ID list (" << cpu_list_str << ").\n";
      return false;
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

  if (profile_config.user_mode_sched) {
    std::cout << "Scheduling: user-mode (group switch every " << profile_config.switch_group_interval << " ms)\n";
  } else {
    std::cout << "Scheduling: kernel (print interval: " << profile_config.switch_group_interval << " ms)\n";
  }

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
  std::cout << "Output file descriptor: " << (profile_config.output_stream ? "set" : "null") << "\n";
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
      long cpu_l = std::strtol(token.c_str(), &endptr, 10);
      if (*endptr != '\0' || cpu_l < 0 || cpu_l > INT_MAX) {
        return std::vector<int>();
      }
      result.push_back(static_cast<int>(cpu_l));
    } else {  // a CPU ID range
      std::string start_str = token.substr(0, dash_pos);
      std::string end_str = token.substr(dash_pos + 1);
      char *endptr1 = nullptr;
      char *endptr2 = nullptr;
      long start_l = std::strtol(start_str.c_str(), &endptr1, 10);
      long end_l = std::strtol(end_str.c_str(), &endptr2, 10);
      if (*endptr1 != '\0' || *endptr2 != '\0' || start_l < 0 || end_l < 0 ||
          end_l < start_l || start_l > INT_MAX || end_l > INT_MAX) {
        return std::vector<int>();
      }
      for (int i = static_cast<int>(start_l); i <= static_cast<int>(end_l); ++i) {
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
      << "      --list-events           Print the PMU event list from the config file and exit.\n"
      << "      --monitor <target>      CMN memory bandwidth monitoring mode (Linux only).\n"
      << "                              Targets: arm_cmn_mem_bw_all, arm_cmn_mem_bw_up, arm_cmn_mem_bw_down\n"
      << "      --cmn-mc-pos <file>     Memory controller position file (required with --monitor).\n"
      << "      --user-mode-sched       Use userspace event group scheduling (legacy).\n"
      << "                              Default: kernel handles multiplexing.\n"
      << "  -h, --help                  Show this help message and exit.\n"
      << "\nExample:\n"
      << "  Specify a PID\n"
      << "    " << program_name << " -p 1234 -d 5 -i 100\n"
      << "  Give a command\n"
      << "    " << program_name << " -i 500 /bin/sleep 10\n"
      << "  System-wide monitor\n"
      << "    " << program_name << " -a -d 10 -i 1000\n"
      << "  CMN bandwidth monitor\n"
      << "    " << program_name << " --monitor arm_cmn_mem_bw_all --cmn-mc-pos mc_pos.txt -d 30 -i 1000\n";
}