#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "pmu_config.h"

/**
 * @brief Structure to hold a single event count for a interval
 */
struct Record {
  uint64_t timestamp;
  int cpu_id;  // -1 for per-process mode
  int group_id;
  uint64_t event_id;
  uint64_t value;
};

/**
 * @brief Structure to hold aggregated statistics for an event
 */
struct EventStats {
  uint64_t total_value;
  uint64_t estimated_value;

  EventStats() : total_value(0), estimated_value(0) {}
};

/**
 * @brief Class for processing raw count and aggregate
 *
 * Supports two modes:
 * - User mode (legacy): estimation uses userspace-tracked time ratios.
 * - Kernel mode: estimation uses kernel's time_enabled/time_running from perf read format.
 *   In kernel mode, stat_[0] holds fixed events, stat_[1..N] holds schedulable event groups.
 */
class Reporter {
 public:
  explicit Reporter(const PMUConfig &pmu_config, bool kernel_mode = false);
  ~Reporter() = default;
  /**
   * @brief Accumulate a single raw counter record into per-group statistics.
   *        Also advances the enabled-time and total-time trackers (user mode only).
   *
   * @param record The raw counter record to process.
   */
  void process_a_record(const Record &record);

  /**
   * @brief Write a single record in CSV format to the given output stream.
   *        Format: timestamp,cpu_id,group,event_name,value
   *
   * @param record The record to print.
   * @param out    The output stream to write to.
   */
  void print_a_record(const Record &record, std::ostream &out);

  /**
   * @brief Apply multiplexing time compensation to produce estimated full-duration values.
   *        Must be called after all records have been processed and before print_stats() / print_metrics().
   */
  void estimation();

  /**
   * @brief Print aggregated event counts (after multiplexing compensation) to stdout.
   */
  void print_stats();

  /**
   * @brief Evaluate the metric formulas defined in the PMU config and print results to stdout.
   */
  void print_metrics();

  /**
   * @brief Accumulate kernel time for a group (kernel mode only).
   *        For multi-CPU scenarios, call once per scheduler; values are summed.
   *
   * @param group_id Group index (0 = fixed events in kernel mode, 1..N = schedulable).
   * @param time_enabled Cumulative time_enabled from perf read format.
   * @param time_running Cumulative time_running from perf read format.
   */
  void add_kernel_time(int group_id, uint64_t time_enabled, uint64_t time_running);

  /**
   * @brief Set the total measurement wall-clock time in nanoseconds (kernel mode).
   */
  void set_total_time(uint64_t total_time_in_ns);

 private:
  const PMUConfig &pmu_config_;

  std::vector<std::vector<EventStats>> stat_;
  std::vector<uint64_t> enabled_time_in_ns_;  // user mode: per-group userspace-tracked time
  uint64_t total_time_in_ns_ = 0;

  uint64_t prev_timestamp_ = 0;

  size_t fixed_event_num_ = 0;

  bool kernel_mode_;
  std::vector<uint64_t> kernel_time_enabled_;  // kernel mode: per-group cumulative time_enabled
  std::vector<uint64_t> kernel_time_running_;  // kernel mode: per-group cumulative time_running

  void estimation_kernel_mode_();

  EventStats get_schedulable_event_stat_by_name(const std::string &name);

  EventStats get_fixed_event_stat_by_name(const std::string &name);

  void print_event_count_(uint64_t c, const std::string &event_name);

  std::string format_with_commas_(uint64_t value);

  void print_metric_(double value, const std::string &type, const std::string &name);

  void print_percentage_(double value, const std::string &metric_name);

  void print_decimal_(double value, const std::string &metric_name);

  void print_cycles_(double value, const std::string &metric_name);

  void print_GHz_(double value, const std::string &metric_name);
};
