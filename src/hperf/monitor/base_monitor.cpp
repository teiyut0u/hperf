#include "hperf/monitor/base_monitor.hpp"

#include <linux/perf_event.h>

#include <cmath>
#include <cstdint>
#include <system_error>
#include <utility>

std::errc BaseMonitor::set_monitor(
    std::vector<SingleEventController>&& controller_vec) {
  single_event_monitor_vec_.clear();
  single_event_monitor_vec_.reserve(controller_vec.size());
  for (auto& controller : controller_vec) {
    if (BaseMonitor::validate_read_format(
            controller.get_read_format())) {
      single_event_monitor_vec_.emplace_back(
          std::move(controller),
          std::array<uint64_t, 3>{});
    } else {
      return std::errc::invalid_argument;
    }
  }
  return std::errc();
}

std::errc BaseMonitor::add_monitor(std::vector<SingleEventController>&& controller_vec) {
  single_event_monitor_vec_.reserve(
      single_event_monitor_vec_.size() + controller_vec.size());
  for (auto&& controller : controller_vec) {
    if (!BaseMonitor::validate_read_format(
            controller.get_read_format())) {
      return std::errc::invalid_argument;
    }
    this->single_event_monitor_vec_.emplace_back(
        std::move(controller),
        std::array<uint64_t, 3>{});
  }
  return std::errc();
}

std::errc BaseMonitor::add_monitor(SingleEventController&& controller) {
  if (!BaseMonitor::validate_read_format(
          controller.get_read_format())) {
    return std::errc::invalid_argument;
  }
  this->single_event_monitor_vec_.emplace_back(
      std::move(controller),
      std::array<uint64_t, 3>{});
  return std::errc();
}

void BaseMonitor::clear() {
  single_event_monitor_vec_.clear();
}

std::error_code BaseMonitor::start() {
  for (auto& monitor : single_event_monitor_vec_) {
    auto reset_err = monitor.first.reset_event();
    if (reset_err) {
      return reset_err;
    }
    auto enable_err = monitor.first.enable_event();
    if (enable_err) {
      return enable_err;
    }
    monitor.second.fill(0);
  }
  return {};
}

std::error_code BaseMonitor::stop() const {
  for (const auto& monitor : single_event_monitor_vec_) {
    auto err = monitor.first.disable_event();
    if (err) {
      return err;
    }
  }
  return {};
}

std::error_code BaseMonitor::get_scaled_count(
    std::vector<uint64_t>& scaled_count_vec) {
  scaled_count_vec.clear();
  scaled_count_vec.reserve(single_event_monitor_vec_.size());
  for (auto& monitor : single_event_monitor_vec_) {
    auto& controller = monitor.first;
    auto& [prev_value, prev_total_time_enabled, prev_total_time_running] = monitor.second;
    // read count
    auto read_err = controller.read_event();
    if (read_err) {
      return read_err;
    }
    // get delta
    uint64_t delta_value = controller.value() - prev_value,
             delta_total_time_enabled = *controller.time_enabled() - prev_total_time_enabled,
             delta_total_time_running = *controller.time_running() - prev_total_time_running;
    // update prev
    prev_value = controller.value();
    prev_total_time_enabled = *controller.time_enabled();
    prev_total_time_running = *controller.time_running();
    // scale count
    uint64_t scaled_count;
    if (delta_total_time_enabled == 0) {
      scaled_count = 0;
    } else {
      scaled_count = static_cast<uint64_t>(
          std::round(
              static_cast<long double>(delta_total_time_enabled) /
              delta_total_time_running *
              delta_value));
    }
    scaled_count_vec.emplace_back(scaled_count);
  }
  return {};
}
