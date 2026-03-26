#ifndef BASE_MONITOR_HPP
#define BASE_MONITOR_HPP

#include <linux/perf_event.h>

#include <array>
#include <cstdint>
#include <system_error>
#include <vector>

#include "hperf/monitor/single_event_controller.hpp"

class BaseMonitor {
 public:
  BaseMonitor() {}

  std::errc set_monitor(
      std::vector<SingleEventController>&& controller_vec);
  std::errc add_monitor(
      std::vector<SingleEventController>&& controller_vec);
  std::errc add_monitor(SingleEventController&& controller);

  void clear();

  std::error_code start();
  std::error_code stop() const;
  std::error_code get_scaled_count(
      std::vector<uint64_t>& scaled_count);

 private:
  inline static bool validate_read_format(uint64_t read_format) {
    return (read_format & PERF_FORMAT_TOTAL_TIME_ENABLED) &&
           (read_format & PERF_FORMAT_TOTAL_TIME_RUNNING);
  }

  // array is: value, total_time_enabled, total_time_running
  template <typename T>
  using monitor_vec = std::vector<std::pair<T, std::array<uint64_t, 3>>>;
  monitor_vec<SingleEventController> single_event_monitor_vec_;
};

#endif
