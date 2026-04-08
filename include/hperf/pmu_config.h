#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "pmu_event.h"  // for struct PMUEvent

/**
 * @brief A single metric item: name, display type, and arithmetic expression.
 */
struct MetricItem {
  std::string name;
  std::string type;  // "decimal" | "percentage" | "GHz" | "cycles"
  std::string expr;
};

/**
 * @brief A named group of metric items displayed under a common section heading.
 */
struct MetricSection {
  std::string title;
  std::vector<MetricItem> items;
};

/**
 * @brief PMU event configuration for the specified CPU, loaded at runtime from a TOML file.
 *
 * Holds fixed events, schedulable event groups, and metric definitions.
 */
class PMUConfig {
 public:
  /**
   * @brief Construct an empty PMUConfig. Call load_from_file() to populate it.
   */
  PMUConfig();

  /**
   * @brief Load the PMU configuration from a TOML file.
   *
   * Populates fixed_events_, event_groups_, and metric_sections_.
   * Also validates that the result is non-empty (equivalent to is_valid()).
   *
   * @param path Absolute or relative path to the .toml config file.
   * @throws HperfError if the file cannot be parsed, a required field is
   *         missing, or the loaded configuration is invalid.
   */
  void load_from_file(const std::string& path);

  /**
   * @brief Check whether the fixed events and the event groups are non-empty.
   *
   * @return true Valid
   * @return false Invalid
   */
  bool is_valid() const;

  /**
   * @brief Get the PMU event based on group index and event index.
   * Indices start from 0; the first events in each group are fixed events.
   *
   * @param group_idx The event group index, starting from 0
   * @param event_idx The event index in the group, starting from 0
   * @return const PMUEvent&
   */
  const PMUEvent& get_pmu_event(size_t group_idx, size_t event_idx) const;

  /**
   * @brief Get the fixed events vector.
   */
  const std::vector<PMUEvent>& get_fixed_events() const;

  /**
   * @brief Get the schedulable event group at the given index.
   * Returns an empty vector and prints an error if the index is out of range.
   *
   * @param idx The event group index, starting from 0
   */
  const std::vector<PMUEvent>& get_event_group_by_idx(size_t idx) const;

  /**
   * @brief Get the number of schedulable event groups.
   */
  size_t get_event_group_num() const;

  /**
   * @brief Get the metric sections loaded from the config file.
   */
  const std::vector<MetricSection>& get_metric_sections() const;

  /**
   * @brief Print the full PMU config to stdout.
   */
  void print_pmu_config() const;

  /**
   * @brief Print each schedulable event group as a single comma-separated line to stdout.
   *        Used to visualize group layout before and after adaptive_grouping().
   */
  void print_event_groups_by_line() const;

  /**
   * @brief Merge event groups greedily while respecting the counter budget.
   *
   * @param programmable_counters_num The number of detected programmable counters
   *        available for schedulable events.
   */
  void adaptive_grouping(size_t programmable_counters_num);

 private:
  std::vector<PMUEvent> fixed_events_;

  /**
   * @brief Schedulable event groups. Each inner vector is one group.
   */
  std::vector<std::vector<PMUEvent>> event_groups_;

  /**
   * @brief Metric definitions loaded from the TOML config file.
   */
  std::vector<MetricSection> metric_sections_;
};
