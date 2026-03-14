#include "hperf/monitor/monitor.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "hperf/monitor/arm_cmn_mem_bandwidth_monitor.hpp"
#include "hperf/monitor/monitor_util.hpp"
#include "hperf/profile_config.h"

std::atomic_bool monitor_enabled{true};

void signal_int_handler(int sig) {
  monitor_enabled.store(false, std::memory_order_release);
}

void setup_signal_int_handler() {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signal_int_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGINT, &sa, nullptr) == -1) {
    perror("sigaction failed");
    _exit(1);
  }
}

void monitor(const ProfileConfig& profile_config) {
  // read memory controller position
  std::ifstream mc_pos_file(profile_config.mc_position_file);
  if (!mc_pos_file.is_open()) {
    std::cerr << "Failed to read memory controller position.\n";
    exit(1);
  }
  std::vector<std::pair<std::filesystem::path, std::vector<uint16_t>>> mc_positions;
  std::string device_name;
  size_t mc_count_on_device;
  while (mc_pos_file >> device_name >> mc_count_on_device) {
    mc_positions.emplace_back(MonitorUtil::DEVICES_DIR / device_name, std::vector<uint16_t>{});
    uint16_t tmp_nodeid_encode;
    for (size_t i = 0; i < mc_count_on_device; ++i) {
      mc_pos_file >> tmp_nodeid_encode;
      mc_positions.back().second.push_back(tmp_nodeid_encode);
    }
  }
  // set output
  std::ofstream output;
  if (profile_config.output_filename.empty()) {
    output.open("/dev/stdout");
  } else {
    output.open(profile_config.output_filename);
  }
  if (!output.is_open()) {
    std::cerr << "Failed to open output file\n.";
    exit(1);
  }

  // add ports to monitor
  ArmCmnMemBWMonitor bw_monitor{profile_config.monitor_target};
  for (const auto& mc_pos : mc_positions) {
    auto err = bw_monitor.add_ports(mc_pos.first, mc_pos.second);
    if (err) {
      std::cerr << "Failed to add posrts because " << err.message() << std::endl;
      exit(1);
    }
  }
  // start monitor
  auto start_err = bw_monitor.start();
  if (start_err) {
    std::cerr << "Failed to start monitoring because " << start_err.message() << std::endl;
    exit(1);
  }
  // set duration
  std::chrono::steady_clock::time_point end_time, next_time;
  if (profile_config.test_duration == -1) {
    end_time = std::chrono::steady_clock::time_point::max();
  } else {
    end_time = std::chrono::steady_clock::now() + std::chrono::seconds(profile_config.test_duration);
  }
  // make sure ellapse some time
  std::this_thread::sleep_for(std::chrono::milliseconds(profile_config.switch_group_interval));
  next_time = std::chrono::steady_clock::now();
  // monitor loop
  setup_signal_int_handler();
  while (next_time < end_time && monitor_enabled.load(std::memory_order_acquire)) {
    uint64_t up_bandwidth, down_bandwidth;
    auto err = bw_monitor.get_bandwidth(up_bandwidth, down_bandwidth);
    if (err) {
      std::cerr << "Failed to get bandwidth because " << err.message() << std::endl;
    }
    // output timestamp
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch())
                  .count();
    output << (ms / 1000) << '.'
           << std::setfill('0') << std::setw(3) << (ms % 1000) << ',';
    // output bandwidth
    MonitorTarget target = bw_monitor.monitor_target();
    if (target == ARM_CMN_MEM_BW_UP || target == ARM_CMN_MEM_BW_ALL) {
      output << up_bandwidth;
    } else {
      output << "Not monitored";
    }
    output << ',';
    if (target == ARM_CMN_MEM_BW_DOWN || target == ARM_CMN_MEM_BW_ALL) {
      output << down_bandwidth;
    } else {
      output << "Not monitored";
    }
    output << std::endl;
    // sleep interval
    next_time += std::chrono::milliseconds(profile_config.switch_group_interval);
    std::this_thread::sleep_until(next_time);
  }
  // stop and exit
  auto stop_err = bw_monitor.stop();
  if (stop_err) {
    std::cerr << "Failed to stop monitor";
  }
  output.close();
  exit(0);
}
