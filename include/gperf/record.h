/*
 * Copyright (c) 2023-2025 Arm Limited.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <unordered_map>

#include "gperf/visualizer.h"
#include "hwcpipe/hwcpipe_counter.h"

/**
 * @brief Record class for accumulating performance data
 *        Maintains cumulative counters for all performance events
 */
class Record {
 public:
  /**
   * @brief Constructor
   */
  Record();

  /**
   * @brief Record (accumulate) performance data from current sample
   *
   * @param data The current sample data as unordered_map with hwcpipe_counter as key
   */
  void record(const std::unordered_map<hwcpipe_counter, CounterValue> &data);

  /**
   * @brief Get accumulated performance data
   *
   * @return Const reference to the accumulated performance data map
   */
  const std::unordered_map<hwcpipe_counter, CounterValue> &get_data() const;

  /**
   * @brief Clear all accumulated data
   */
  void clear();

 private:
  std::unordered_map<hwcpipe_counter, CounterValue> accumulated_data_;

  /**
   * @brief Recalculate utilization metrics from accumulated active cycles
   *        Updates MaliBinningQueueUtil, MaliMainQueueUtil, MaliCompQueueUtil
   *        based on their respective active cycles divided by GPU total active cycles
   */
  void recalculate_utilization();
};
