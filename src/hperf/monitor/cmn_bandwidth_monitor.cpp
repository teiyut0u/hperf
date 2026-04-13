#include "hperf/monitor/cmn_bandwidth_monitor.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

#include "hperf/monitor/event_controller_interface.h"
#include "hperf/monitor/general_monitor.h"
#include "hperf/monitor/monitor_util.h"
#include "hperf/monitor/single_event_controller.h"
#include "hperf/profile_config.h"

static std::atomic_bool s_monitor_enabled{true};

static void monitor_signal_handler(int) {
  s_monitor_enabled.store(false, std::memory_order_release);
}

static void setup_monitor_signal_handler() {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = monitor_signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = SA_RESTART;
  if (sigaction(SIGINT, &sa, nullptr) == -1) {
    perror("sigaction failed");
  }
}

static std::errc read_mc_pos_file(const std::string& mc_pos_file_path,
                                  std::vector<std::pair<std::filesystem::path, std::vector<uint16_t>>>& mc_positions) {
  std::ifstream mc_pos_file(mc_pos_file_path);
  if (!mc_pos_file.is_open()) {
    std::cerr << "Error: Failed to open memory controller position file: " << mc_pos_file_path << "\n";
    return std::errc::no_such_file_or_directory;
  }
  std::string device_name;
  size_t mc_count_on_device;
  std::string nodeid_input_buffer;
  while (mc_pos_file >> device_name >> mc_count_on_device) {
    mc_positions.emplace_back(MonitorUtil::DEVICES_DIR / device_name, std::vector<uint16_t>{});
    uint16_t nodeid_encode;
    for (size_t i = 0; i < mc_count_on_device; ++i) {
      mc_pos_file >> nodeid_input_buffer;
      auto errc = MonitorUtil::string2integer(nodeid_input_buffer, nodeid_encode);
      if (errc != std::errc()) {
        return errc;
      }
      mc_positions.back().second.push_back(nodeid_encode);
    }
  }
  return std::errc();
}

static std::error_code add_arm_cmn_mem_bw_monitor(
    const MonitorTarget monitor_target,
    const std::vector<std::pair<std::filesystem::path, std::vector<uint16_t>>>& mc_positions,
    std::vector<GeneralMonitor>& monitor_vec) {
  // set events
  std::vector<std::string_view> event_vec;
  if (monitor_target == MonitorTarget::ARM_CMN_MEM_BW_UP ||
      monitor_target == MonitorTarget::ARM_CMN_MEM_BW_ALL) {
    event_vec.emplace_back("watchpoint_up");
  } else {
    event_vec.emplace_back();
  }
  if (monitor_target == MonitorTarget::ARM_CMN_MEM_BW_DOWN ||
      monitor_target == MonitorTarget::ARM_CMN_MEM_BW_ALL) {
    event_vec.emplace_back("watchpoint_down");
  } else {
    event_vec.emplace_back();
  }
  // construct monitor
  for (const auto& event : event_vec) {
    if (event.empty()) {
      monitor_vec.emplace_back();
      continue;
    }
    std::vector<std::unique_ptr<EventControllerInterface>> event_controller_ptr_vec;
    for (const auto& mc_pos : mc_positions) {
      for (const auto& nodeid_encode : mc_pos.second) {
        // make attr
        PerfEventAttr attr;
        auto attr_err = MonitorUtil::add_watchpoint_monitor_attr(
            mc_pos.first, nodeid_encode, event, attr);
        if (attr_err) {
          return attr_err;
        }
        // open event
        auto controller_ptr = std::make_unique<SingleEventController>();
        auto event_err = controller_ptr->open_event(attr.get(), -1, 0, 0);
        if (event_err) {
          return event_err;
        }
        // add event
        event_controller_ptr_vec.emplace_back(std::move(controller_ptr));
      }
    }
    // add monitor
    auto add_monitor_errc =
        monitor_vec.emplace_back()
            .add_controller(std::move(event_controller_ptr_vec));
    if (add_monitor_errc != std::errc()) {
      return std::make_error_code(add_monitor_errc);
    }
  }
  return {};
}

static void do_monitor(std::vector<GeneralMonitor>& monitor_vec, int interval,
                       const std::chrono::steady_clock::time_point& end_time,
                       std::ostream& output) {
  const double interval_in_sec = double(interval) / 1000;
  auto next_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(interval);
  while (next_time < end_time && s_monitor_enabled.load(std::memory_order_acquire)) {
    // output timestamp
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::system_clock::now().time_since_epoch())
                  .count();
    output << (ms / 1000) << '.'
           << std::setfill('0') << std::setw(3) << (ms % 1000);
    // output monitor value
    for (auto& monitor : monitor_vec) {
      std::vector<uint64_t> scaled_count;
      auto monitor_err = monitor.get_scaled_count(scaled_count);
      if (monitor_err) {
        std::cerr << "Failed to get monitor value: " << monitor_err.message() << "\n";
      }
      output << ',';
      if (scaled_count.empty()) {
        output << "none";
      } else {
        // get total data flit count, each flit is 32 bytes
        output << static_cast<uint64_t>(
            (std::accumulate(scaled_count.cbegin(), scaled_count.cend(), 0UL) << 5) /
            interval_in_sec);
      }
    }
    output << '\n';
    // sleep interval
    std::this_thread::sleep_until(next_time);
    next_time += std::chrono::milliseconds(interval);
  }
}

bool cmn_bandwidth_monitor(const ProfileConfig& profile_config) {
  // read memory controller position
  std::vector<std::pair<std::filesystem::path, std::vector<uint16_t>>> mc_positions;
  auto read_mc_pos_errc = read_mc_pos_file(profile_config.mc_position_file, mc_positions);
  if (read_mc_pos_errc != std::errc()) {
    std::cerr << "Error: Memory controller position file is corrupted.\n";
    return false;
  }

  // set output
  std::ofstream output_file;
  std::ostream* output = &std::cout;
  if (!profile_config.output_filename.empty()) {
    output_file.open(profile_config.output_filename);
    if (!output_file.is_open()) {
      std::cerr << "Error: Failed to open output file: " << profile_config.output_filename << "\n";
      return false;
    }
    output = &output_file;
  }

  // add monitor
  std::vector<GeneralMonitor> monitor_vec;
  auto monitor_err = add_arm_cmn_mem_bw_monitor(profile_config.monitor_target, mc_positions, monitor_vec);
  if (monitor_err) {
    std::cerr << "Error: Failed to add monitors: " << monitor_err.message() << "\n";
    return false;
  }

  // start monitor
  for (auto& monitor : monitor_vec) {
    auto start_err = monitor.start();
    if (start_err) {
      std::cerr << "Error: Failed to start monitoring: " << start_err.message() << "\n";
      return false;
    }
  }

  // make sure some time elapses before first read
  std::this_thread::sleep_for(std::chrono::milliseconds(profile_config.switch_group_interval));

  // set duration
  std::chrono::steady_clock::time_point end_time;
  if (profile_config.test_duration == -1) {
    end_time = std::chrono::steady_clock::time_point::max();
  } else {
    end_time = std::chrono::steady_clock::now() + std::chrono::seconds(profile_config.test_duration + 1);
  }
  setup_monitor_signal_handler();

  std::cout << "CMN bandwidth monitoring started...\n";
  do_monitor(monitor_vec, profile_config.switch_group_interval, end_time, *output);

  // stop and cleanup
  for (const auto& monitor : monitor_vec) {
    auto stop_err = monitor.stop();
    if (stop_err) {
      std::cerr << "Warning: Failed to stop monitor: " << stop_err.message() << "\n";
    }
  }

  if (s_monitor_enabled.load(std::memory_order_acquire)) {
    std::cout << "CMN bandwidth monitoring finished.\n";
  } else {
    std::cout << "\nInterrupted. CMN bandwidth monitoring stopped.\n";
  }

  return true;
}
