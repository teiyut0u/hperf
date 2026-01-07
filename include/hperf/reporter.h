#pragma once

#include <csignal>
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
 */
class Reporter {
 public:
  Reporter(const PMUConfig &pmu_config);
  ~Reporter();
  void process_a_record(const Record &record);
  void print_a_record(const Record &record, std::ostream &out);
  void estimation();
  void print_stats();
  void print_metrics();

 private:
  const PMUConfig &pmu_config_;

  std::vector<std::vector<EventStats>> stat_;
  std::vector<uint64_t> enabled_time_in_ns_;
  uint64_t total_time_in_ns_;

  uint64_t prev_timestamp_;

  int fixed_event_num_;

  EventStats get_event_stat_by_name(std::string name, size_t group_id);

  EventStats get_schedulable_event_stat_by_name(std::string name, size_t &group_id);

  EventStats get_fixed_event_stat_by_name(std::string name, size_t group_id);

  void print_event_count_(uint64_t c, std::string event_name);

  std::string format_with_commas_(uint64_t value);

  void print_percentage_(uint64_t a, uint64_t b, std::string metric_name);

  void print_decimal_(uint64_t a, uint64_t b, std::string metric_name);

  void print_cycles_(uint64_t a, uint64_t b, std::string metric_name);

  void print_GHz_(uint64_t a, uint64_t b, std::string metric_name);

  void print_metrics_oryon_();

  void print_metrics_cortex_x4_();
};
