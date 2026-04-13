#pragma once

#include <linux/perf_event.h>

#include <cstdint>
#include <memory>
#include <system_error>
#include <tuple>
#include <vector>

#include "hperf/monitor/event_controller_interface.h"

class GeneralMonitor {
 public:
  GeneralMonitor() {}

  std::errc set_controller(
      std::vector<std::unique_ptr<EventControllerInterface>>&& controller_ptr_vec);
  std::errc add_controller(
      std::vector<std::unique_ptr<EventControllerInterface>>&& controller_vec);
  std::errc add_controller(std::unique_ptr<EventControllerInterface>&& controller);

  void clear();

  std::error_code start() const;
  std::error_code stop() const;
  void reset();

  std::vector<size_t> get_size() const;
  std::error_code get_scaled_count(
      std::vector<uint64_t>& scaled_count);

 private:
  inline static bool validate_read_format(uint64_t read_format) {
    return (read_format & PERF_FORMAT_TOTAL_TIME_ENABLED) &&
           (read_format & PERF_FORMAT_TOTAL_TIME_RUNNING);
  }

  void add_scaled_count(std::vector<uint64_t>& scaled_count_vec);

  // tuple is: all value, total_time_enabled, total_time_running
  template <typename T>
  using monitor_vec = std::vector<std::pair<std::unique_ptr<T>, std::tuple<std::vector<uint64_t>, uint64_t, uint64_t>>>;
  monitor_vec<EventControllerInterface> event_monitor_vec_;
};
