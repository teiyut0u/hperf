#ifndef ARM_CMN_MEM_BANDWIDTH_MONITOR_HPP
#define ARM_CMN_MEM_BANDWIDTH_MONITOR_HPP

#include <linux/perf_event.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <system_error>
#include <tuple>
#include <vector>

#include "hperf/monitor/perf_event_attr.hpp"
#include "hperf/monitor/single_event_controller.hpp"
#include "hperf/profile_config.h"

namespace fs = std::filesystem;

class ArmCmnMemBWMonitor {
 public:
  ArmCmnMemBWMonitor(MonitorTarget monitor_target) : monitor_targrt(monitor_target) {}

  std::error_code add_ports(const fs::path& device_path, const std::vector<uint16_t>& nodeid_encode_vec);

  void reset();

  std::error_code start();
  std::error_code stop();
  std::error_code get_bandwidth(uint64_t& up_bandwidth, uint64_t& down_bandwidth);

  MonitorTarget monitor_target() const { return monitor_targrt; }

 private:
  std::error_code add_attr_field(const fs::path device_path, uint16_t nodeid_encode, std::string_view event_name, PerfEventAttr& attr);

  std::error_code start_helper(std::vector<std::pair<SingleEventController, std::tuple<uint64_t, uint64_t, uint64_t>>>& vec);

  std::error_code stop_helper(std::vector<std::pair<SingleEventController, std::tuple<uint64_t, uint64_t, uint64_t>>>& vec);

  std::error_code get_bandwidth_helper(uint64_t& bandwidth, std::vector<std::pair<SingleEventController, std::tuple<uint64_t, uint64_t, uint64_t>>>& vec);

  std::vector<std::pair<SingleEventController, std::tuple<uint64_t, uint64_t, uint64_t>>> up_monitor_vec_, down_monitor_vec_;
  MonitorTarget monitor_targrt;
};

#endif
