/*
 * Copyright (c) 2023-2025 Arm Limited.
 *
 * SPDX-License-Identifier: MIT
 */

#include "gperf/record.h"

Record::Record() : accumulated_data_() {}

void Record::record(const std::unordered_map<hwcpipe_counter, CounterValue> &data) {
  for (const auto &entry : data) {
    hwcpipe_counter counter = entry.first;
    const CounterValue &value = entry.second;

    auto it = accumulated_data_.find(counter);
    if (it == accumulated_data_.end()) {
      // First occurrence of this counter, just insert it
      accumulated_data_[counter] = value;
    } else {
      // Accumulate the value
      // Special case: utilization counters should not be accumulated, just keep last value
      if (counter == MaliBinningQueueUtil || counter == MaliMainQueueUtil ||
          counter == MaliCompQueueUtil) {
        accumulated_data_[counter] = value;
      } else {
        // For all other counters, accumulate the values
        if (value.type == CounterValue::UINT64) {
          uint64_t current = it->second.as_uint64();
          uint64_t new_val = value.as_uint64();
          accumulated_data_[counter] = CounterValue(current + new_val);
        } else {
          // For double counters (like rates), accumulate them
          double current = it->second.as_double();
          double new_val = value.as_double();
          accumulated_data_[counter] = CounterValue(current + new_val);
        }
      }
    }
  }
}

void Record::recalculate_utilization() {
  // Get GPU total active cycles
  uint64_t gpu_active_cycles = 0;
  auto it = accumulated_data_.find(MaliGPUActiveCy);
  if (it != accumulated_data_.end()) {
    gpu_active_cycles = it->second.as_uint64();
  }

  if (gpu_active_cycles == 0) {
    return;  // Avoid division by zero
  }

  // Recalculate Binning Queue Utilization
  auto binning_it = accumulated_data_.find(MaliBinningQueueActiveCy);
  if (binning_it != accumulated_data_.end()) {
    uint64_t binning_active_cycles = binning_it->second.as_uint64();
    double binning_util = static_cast<double>(binning_active_cycles) / static_cast<double>(gpu_active_cycles) * 100.0;
    accumulated_data_[MaliBinningQueueUtil] = CounterValue(binning_util);
  }

  // Recalculate Main Queue Utilization
  auto main_it = accumulated_data_.find(MaliMainQueueActiveCy);
  if (main_it != accumulated_data_.end()) {
    uint64_t main_active_cycles = main_it->second.as_uint64();
    double main_util = static_cast<double>(main_active_cycles) / static_cast<double>(gpu_active_cycles) * 100.0;
    accumulated_data_[MaliMainQueueUtil] = CounterValue(main_util);
  }

  // Recalculate Compute Queue Utilization
  auto compute_it = accumulated_data_.find(MaliCompQueueActiveCy);
  if (compute_it != accumulated_data_.end()) {
    uint64_t compute_active_cycles = compute_it->second.as_uint64();
    double compute_util = static_cast<double>(compute_active_cycles) / static_cast<double>(gpu_active_cycles) * 100.0;
    accumulated_data_[MaliCompQueueUtil] = CounterValue(compute_util);
  }
}

const std::unordered_map<hwcpipe_counter, CounterValue> &Record::get_data() const {
  // Recalculate utilization metrics before returning
  const_cast<Record *>(this)->recalculate_utilization();
  return accumulated_data_;
}

void Record::clear() {
  accumulated_data_.clear();
}
