#ifndef MOCK_EVENT_CONTROLLER_HPP
#define MOCK_EVENT_CONTROLLER_HPP

#include <cstdint>
#include <optional>
#include <system_error>
#include <vector>

#include "hperf/monitor/event_controller/event_controller_interface.hpp"

/**
 * @brief Mock implementation of EventControllerInterface for testing.
 *
 * This allows tests to control all aspects of the event controller:
 * - control operations (enable, disable, reset)
 * - read operations and data
 * - timing information
 */
class MockEventController : public EventControllerInterface {
 public:
  MockEventController();

  // Override virtual methods
  std::error_code control(unsigned long request, void* arg) const override;
  std::error_code read() override;
  std::error_code close() override;

  size_t size() const override { return size_; }
  std::vector<uint64_t> all_value() const override { return values_; }
  std::optional<uint64_t> time_enabled() const override { return time_enabled_; }
  std::optional<uint64_t> time_running() const override { return time_running_; }
  std::optional<std::vector<uint64_t>> all_id() const override { return ids_; }
  std::optional<std::vector<uint64_t>> all_lost() const override { return lost_; }

  uint64_t get_read_format() const override { return read_format_; }

  // Test configuration methods
  void set_size(size_t size) { size_ = size; }
  void set_values(std::vector<uint64_t> values) { values_ = std::move(values); }
  void set_time_enabled(uint64_t time) { time_enabled_ = time; }
  void set_time_running(uint64_t time) { time_running_ = time; }
  void set_ids(std::vector<uint64_t> ids) { ids_ = std::move(ids); }
  void set_lost(std::vector<uint64_t> lost) { lost_ = std::move(lost); }
  void set_read_format(uint64_t fmt) { read_format_ = fmt; }

  // Mock control behavior
  void set_control_error(std::error_code ec) { control_error_ = ec; }
  void set_read_error(std::error_code ec) { read_error_ = ec; }
  void set_close_error(std::error_code ec) { close_error_ = ec; }

  // Track method calls
  int get_control_call_count() const { return control_call_count_; }
  int get_read_call_count() const { return read_call_count_; }
  int get_close_call_count() const { return close_call_count_; }

  void reset_call_counts() {
    control_call_count_ = 0;
    read_call_count_ = 0;
    close_call_count_ = 0;
  }

 private:
  // Data members
  size_t size_ = 1;
  std::vector<uint64_t> values_ = {100};
  std::optional<uint64_t> time_enabled_ = 1000;
  std::optional<uint64_t> time_running_ = 1000;
  std::optional<std::vector<uint64_t>> ids_ = std::vector<uint64_t>{1};
  std::optional<std::vector<uint64_t>> lost_;
  uint64_t read_format_ = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

  // Error simulation
  std::error_code control_error_;
  std::error_code read_error_;
  std::error_code close_error_;

  // Call counters
  mutable int control_call_count_ = 0;
  int read_call_count_ = 0;
  int close_call_count_ = 0;
};

#endif
