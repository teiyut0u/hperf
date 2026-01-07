#pragma once

#include <linux/perf_event.h>  // for PERF_TYPE_* marcos

#include "pmu_event.h"  // for struct PMUEvent

#include <cstddef>
#include <vector>

/**
 * @brief PMU event configuration for the specified CPU, including the static information about event groups and events. 
 * 
 */
class PMUConfig {
 public:
  /**
   * @brief Construct a new PMUConfig object
   *
   * Loads the PMU config based on the CPU type defined at compile time. 
   */
  PMUConfig();

  /**
   * @brief Check whether the fixed events and the event groups are empty
   * 
   * @return true Valid
   * @return false Invalid
   */
  bool is_valid() const;

  /**
   * @brief Get the PMU event based on group index and event index.
   * Indices start from 0, and also note that the first events in each event group are fixed events. 
   *
   * @param group_idx The event group index, starting from 0
   * @param event_idx The event index in the group, starting from 0
   * @return const PMUEvent& 
   */
  const PMUEvent& get_pmu_event(size_t group_idx, size_t event_idx) const;

  /**
   * @brief Get the vector of PMU event information of the fixed events. 
   * 
   * @return const std::vector<PMUEvent>& 
   */
  const std::vector<PMUEvent>& get_fixed_events() const;

  /**
   * @brief Get the vector of PMU event information of the specified event group specified by the index. 
   * If the index is invalid, an empty vector will be returned. 
   * 
   * @param idx The event group index, starting from 0
   * @return const std::vector<PMUEvent>& 
   */
  const std::vector<PMUEvent>& get_event_group_by_idx(size_t idx) const;

  /**
   * @brief Get the number of event groups
   * 
   * @return size_t The number of event groups
   */
  size_t get_event_group_num() const;

  /**
   * @brief Print the PMU config to the console
   *
   * Display the events information (name, encoding, description) in each event group. 
   */
  void print_pmu_config() const;

  void print_event_groups_by_line() const;

  /**
   * @brief Optimize event groups by merging the original event groups as many as possible.
   * The optimization will modified the original event groups. 
   * 
   * @param programmable_counter_num The number of the detected programmable counters
   */
  void adaptive_grouping(size_t programmable_counters_num); 

 private:
  /**
   * @brief 
   * 
   */
  std::vector<PMUEvent> fixed_events_;

  /**
   * @brief The PMU config loaded from CPU-specific header file
   *
   * Each inner vector represents an event group, and its inner vector represent an event config in this event group.
   */
  std::vector<std::vector<PMUEvent>> event_groups_;
};
