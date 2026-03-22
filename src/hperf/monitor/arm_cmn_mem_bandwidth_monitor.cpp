#include "hperf/monitor/arm_cmn_mem_bandwidth_monitor.hpp"

#include <linux/perf_event.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "hperf/monitor/monitor_util.hpp"
#include "hperf/monitor/perf_event_attr.hpp"
#include "hperf/profile_config.h"

std::error_code ArmCmnMemBWMonitor::add_ports(const fs::path& device_path, const std::vector<uint16_t>& nodeid_encode_vec) {
  // printf("device_path '%s'\n", device_path.string().c_str());
  std::vector<std::string_view> events;
  // reserve space & add events
  if (monitor_targrt == ARM_CMN_MEM_BW_UP || monitor_targrt == ARM_CMN_MEM_BW_ALL) {
    down_monitor_vec_.reserve(down_monitor_vec_.size() + nodeid_encode_vec.size());
    events.emplace_back("watchpoint_up");
  }
  if (monitor_targrt == ARM_CMN_MEM_BW_DOWN || monitor_targrt == ARM_CMN_MEM_BW_ALL) {
    up_monitor_vec_.reserve(up_monitor_vec_.size() + nodeid_encode_vec.size());
    events.emplace_back("watchpoint_down");
  }
  // add controllers
  for (const auto& event : events) {
    for (auto nodeid_encode : nodeid_encode_vec) {
      // make attr
      PerfEventAttr attr;
      auto err = add_attr_field(device_path, nodeid_encode, event, attr);
      // MonitorUtil::debug_perf_event_attr(*attr.get());
      if (err) {
        // printf("Failed to add_attr_field\n");
        return err;
      }
      // open event
      auto& vec = (event == "watchpoint_up") ? up_monitor_vec_ : down_monitor_vec_;
      vec.emplace_back();
      auto event_err = vec.back().first.open_event(attr.get(), -1, 0, 0);
      if (event_err) {
        // printf("Failed to open event\n");
        return event_err;
      }
    }
  }
  return std::error_code{};
}

void ArmCmnMemBWMonitor::reset() {
  up_monitor_vec_.resize(0);
  down_monitor_vec_.resize(0);
}

std::error_code ArmCmnMemBWMonitor::start() {
  auto up_err = start_helper(up_monitor_vec_);
  if (up_err) {
    return up_err;
  }
  auto down_err = start_helper(down_monitor_vec_);
  if (down_err) {
    return down_err;
  }
  return {};
}

std::error_code ArmCmnMemBWMonitor::start_helper(std::vector<std::pair<SingleEventController, std::tuple<uint64_t, uint64_t, uint64_t>>>& vec) {
  for (auto& monitor : vec) {
    auto reset_err = monitor.first.reset_event();
    if (reset_err) {
      return reset_err;
    }
    auto enable_err = monitor.first.enable_event();
    if (enable_err) {
      return enable_err;
    }
  }
  return std::error_code{};
}

std::error_code ArmCmnMemBWMonitor::stop() {
  auto up_err = stop_helper(up_monitor_vec_);
  if (up_err) {
    return up_err;
  }
  auto down_err = stop_helper(up_monitor_vec_);
  if (down_err) {
    return down_err;
  }
  return {};
}

std::error_code ArmCmnMemBWMonitor::stop_helper(std::vector<std::pair<SingleEventController, std::tuple<uint64_t, uint64_t, uint64_t>>>& vec) {
  for (auto& monitor : vec) {
    auto err = monitor.first.disable_event();
    if (err) {
      return err;
    }
  }
  return std::error_code{};
}

std::error_code ArmCmnMemBWMonitor::get_bandwidth(uint64_t& up_bandwidth, uint64_t& down_bandwidth) {
  auto up_err = get_bandwidth_helper(up_bandwidth, up_monitor_vec_);
  if (up_err) {
    return up_err;
  }
  auto down_err = get_bandwidth_helper(down_bandwidth, down_monitor_vec_);
  if (down_err) {
    return down_err;
  }
  return std::error_code{};
}

std::error_code ArmCmnMemBWMonitor::get_bandwidth_helper(uint64_t& bandwidth, std::vector<std::pair<SingleEventController, std::tuple<uint64_t, uint64_t, uint64_t>>>& vec) {
  uint64_t delta_total_value = 0, delta_total_time_enabled = 0, delta_total_time_running = 0;
  for (auto& monitor : vec) {
    auto read_err = monitor.first.read_event();
    if (read_err) {
      return read_err;
    }
    delta_total_value +=
        monitor.first.value() - std::get<0>(monitor.second);
    std::get<0>(monitor.second) = monitor.first.value();

    delta_total_time_enabled +=
        *monitor.first.time_enabled() - std::get<1>(monitor.second);
    std::get<1>(monitor.second) = *monitor.first.time_enabled();

    delta_total_time_running +=
        *monitor.first.time_running() - std::get<2>(monitor.second);
    std::get<2>(monitor.second) = *monitor.first.time_running();
  }
  if (delta_total_time_enabled == 0) {
    bandwidth = 0;
  } else {
    bandwidth = static_cast<uint64_t>(
        std::round(
            static_cast<long double>(delta_total_time_enabled) /
            delta_total_time_running *
            delta_total_value));
  };
  bandwidth <<= 5;  // 32 bytes per flit
  // printf("enabled '%lu',running '%lu',value '%lu',bandwidth '%lu'\n", delta_total_time_enabled, delta_total_time_running, delta_total_value, bandwidth);
  return std::error_code{};
}

std::error_code ArmCmnMemBWMonitor::add_attr_field(const fs::path device_path, uint16_t nodeid_encode, std::string_view event_name, PerfEventAttr& attr) {
  attr.set_disabled();
  attr.set_read_format(PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING);
  // add type
  auto type_err = attr.set_type(device_path / "type");
  if (type_err) {
    // printf("Failed to add_type\n");
    return type_err;
  }
  // add event
  auto event_err = attr.add_event(device_path / "events" / event_name);
  if (event_err) {
    // printf("Failed to add_event\n");
    return event_err;
  }
  // add param
  std::array<std::pair<std::string_view, uint64_t>, 6> params{{
      {"bynodeid", 0x1},
      {"nodeid", nodeid_encode & (uint64_t(-1) << 3)},
      {"wp_dev_sel", (nodeid_encode & 0x4) >> 2},
      {"wp_chn_sel", 0x3},
      {"wp_val", 0x0},
      {"wp_mask", 0xffffffffffffffff},
  }};
  fs::path format_path = device_path / "format";
  for (const auto& param : params) {
    auto field_err = attr.add_field(format_path / param.first, param.second);
    if (field_err) {
      // printf("Failed to add_field\n");
      return field_err;
    }
  }
  return std::error_code{};
}
