#include "hperf/monitor/general_monitor.h"

#include <linux/perf_event.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <system_error>
#include <tuple>
#include <utility>

#include "hperf/monitor/event_controller_interface.h"

std::errc GeneralMonitor::set_controller(
    std::vector<std::unique_ptr<EventControllerInterface>>&& controller_ptr_vec) {
  event_monitor_vec_.clear();
  event_monitor_vec_.reserve(controller_ptr_vec.size());
  for (auto& controller_ptr : controller_ptr_vec) {
    if (GeneralMonitor::validate_read_format(
            controller_ptr->get_read_format())) {
      size_t controller_size = controller_ptr->size();
      event_monitor_vec_.emplace_back(
          std::move(controller_ptr),
          std::make_tuple(
              std::vector<uint64_t>(controller_size, 0), 0, 0));
    } else {
      return std::errc::invalid_argument;
    }
  }
  return std::errc();
}

std::errc GeneralMonitor::add_controller(std::vector<std::unique_ptr<EventControllerInterface>>&& controller_ptr_vec) {
  event_monitor_vec_.reserve(
      event_monitor_vec_.size() + controller_ptr_vec.size());
  for (auto&& controller_ptr : controller_ptr_vec) {
    if (!GeneralMonitor::validate_read_format(
            controller_ptr->get_read_format())) {
      return std::errc::invalid_argument;
    }
    size_t controller_size = controller_ptr->size();
    event_monitor_vec_.emplace_back(
        std::move(controller_ptr),
        std::make_tuple(
            std::vector<uint64_t>(controller_size, 0), 0, 0));
  }
  return std::errc();
}

std::errc GeneralMonitor::add_controller(std::unique_ptr<EventControllerInterface>&& controller_ptr) {
  if (!GeneralMonitor::validate_read_format(
          controller_ptr->get_read_format())) {
    return std::errc::invalid_argument;
  }
  size_t controller_size = controller_ptr->size();
  event_monitor_vec_.emplace_back(
      std::move(controller_ptr),
      std::make_tuple(
          std::vector<uint64_t>(controller_size, 0), 0, 0));
  return std::errc();
}

void GeneralMonitor::clear() {
  event_monitor_vec_.clear();
}

std::error_code GeneralMonitor::start() const {
  for (auto& monitor : event_monitor_vec_) {
    auto reset_err = monitor.first->reset();
    if (reset_err) {
      return reset_err;
    }
    auto enable_err = monitor.first->enable();
    if (enable_err) {
      return enable_err;
    }
  }
  return {};
}

std::error_code GeneralMonitor::stop() const {
  for (const auto& monitor : event_monitor_vec_) {
    auto err = monitor.first->disable();
    if (err) {
      return err;
    }
  }
  return {};
}

void GeneralMonitor::reset() {
  for (auto& monitor : event_monitor_vec_) {
    for (auto& prev_value_item : std::get<0>(monitor.second)) {
      prev_value_item = 0;
    }
    std::get<1>(monitor.second) = 0;
    std::get<2>(monitor.second) = 0;
  }
}

std::vector<size_t> GeneralMonitor::get_size() const {
  std::vector<size_t> result;
  for (const auto& monitor : event_monitor_vec_) {
    result.push_back(monitor.first->size());
  }
  return result;
}

std::error_code GeneralMonitor::get_scaled_count(
    std::vector<uint64_t>& scaled_count_vec) {
  scaled_count_vec.clear();
  scaled_count_vec.reserve(event_monitor_vec_.size());
  // read count
  for (auto& monitor : event_monitor_vec_) {
    auto read_err = monitor.first->read();
    if (read_err) {
      return read_err;
    }
  }
  // add scaled count
  add_scaled_count(scaled_count_vec);
  return {};
}

void GeneralMonitor::add_scaled_count(std::vector<uint64_t>& scaled_count_vec) {
  for (auto& monitor : event_monitor_vec_) {
    auto& controller_ptr = monitor.first;
    auto& [all_prev_value, prev_time_enabled, prev_time_running] = monitor.second;
    // get and update delta time
    uint64_t delta_time_enabled =
                 *controller_ptr->time_enabled() - prev_time_enabled,
             delta_time_running =
                 *controller_ptr->time_running() - prev_time_running;
    prev_time_enabled = *controller_ptr->time_enabled();
    prev_time_running = *controller_ptr->time_running();
    // get all delta value, scale, and update prev
    auto all_current_value = controller_ptr->all_value();
    for (size_t i = 0; i < all_current_value.size(); ++i) {
      uint64_t scaled_count;
      if (delta_time_enabled == 0) {
        scaled_count = 0;
      } else {
        scaled_count = static_cast<uint64_t>(
            std::round(
                static_cast<long double>(delta_time_enabled) /
                delta_time_running *
                (all_current_value[i] - all_prev_value[i])));
      }
      scaled_count_vec.emplace_back(scaled_count);
    }
    all_prev_value = std::move(all_current_value);
  }
}
