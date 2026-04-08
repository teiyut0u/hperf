/*
 * Copyright (c) 2023-2025 Arm Limited.
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <variant>

#include "hwcpipe/hwcpipe_counter.h"

/**
 * @brief Counter value that can hold either uint64_t or double
 */
struct CounterValue {
  enum Type { UINT64,
              FLOAT64 } type;
  std::variant<uint64_t, double> value;

  CounterValue() : type(UINT64), value(uint64_t(0)) {}

  explicit CounterValue(uint64_t v) : type(UINT64), value(v) {}

  explicit CounterValue(double v) : type(FLOAT64), value(v) {}

  /**
   * @brief Get value as double
   */
  double as_double() const {
    if (type == UINT64) {
      return static_cast<double>(std::get<uint64_t>(value));
    } else {
      return std::get<double>(value);
    }
  }

  /**
   * @brief Get value as uint64_t
   */
  uint64_t as_uint64() const {
    if (type == UINT64) {
      return std::get<uint64_t>(value);
    } else {
      return static_cast<uint64_t>(std::get<double>(value));
    }
  }
};

/**
 * @brief Visualizer class for GPU performance data
 *        Displays performance metrics in a formatted console layout
 */
class Visualizer {
 public:
  /**
   * @brief Visualize GPU performance data on console
   *        Clears previous output and displays current performance metrics
   *
   * @param data An unordered_map with hwcpipe_counter as key and counter values
   * @param interval Time interval in nanoseconds since last sample
   */
  void visualize(const std::unordered_map<hwcpipe_counter, CounterValue> &data,
                 uint64_t interval);

 private:
  /**
   * @brief Format cycles value for display
   *
   * @param cycles The cycle count
   * @return Formatted string (in thousands with 3 decimal places)
   */
  std::string format_cycles(double cycles) const;

  /**
   * @brief Format uint64_t value for display
   *
   * @param value The uint64_t value
   * @param width The minimum width (at least 7, default 12)
   * @return Formatted string with specified width
   */
  std::string format_uint64(uint64_t value, int width = 12) const;

  /**
   * @brief Format rate value for display
   *
   * @param rate The rate value
   * @param width The total width (at least 9, default 9). Number width is width - 3 for " /s"
   * @return Formatted string with specified total width (number + " /s")
   */
  std::string format_rate(double rate, int width = 9) const;

  /**
   * @brief Format MHz value for display
   *
   * @param mhz The frequency in MHz
   * @return Formatted string with total width of 12 (8 digits + " MHz")
   */
  std::string format_mhz(double mhz) const;

  /**
   * @brief Format utilization percentage for display
   *
   * @param util The utilization value
   * @return Formatted string (6 characters with 2 decimal places)
   */
  std::string format_util(double util) const;

  /**
   * @brief Format bandwidth for display
   *
   * @param mib_s The bandwidth in MiB/s
   * @return Formatted string (9 digits with 3 decimal places + " MiB/s", total width 15)
   */
  std::string format_bandwidth(double mib_s) const;

  /**
   * @brief Format memory read latency for display
   *
   * @param latency The latency in cycles
   * @return Formatted string (8 digits with 2 decimal places + " Cycles", total width 15)
   */
  std::string format_latency(double latency) const;

  /**
   * @brief Get value from data map with default
   *
   * @param data The data unordered_map
   * @param key The counter ID to look up
   * @param default_val Default value if key not found
   * @return The CounterValue
   */
  CounterValue get_value(const std::unordered_map<hwcpipe_counter, CounterValue> &data,
                         hwcpipe_counter key, const CounterValue &default_val = CounterValue()) const;
};

/**
 * @brief Calculate event frequency in MHz
 *
 * @param cycles Event count value
 * @param interval_ns Time interval in nanoseconds
 * @return Event frequency in MHz, or 0.0 if interval is 0
 */
inline double calculate_mhz(uint64_t cycles, uint64_t interval_ns) {
  if (interval_ns == 0) {
    return 0.0;
  }
  // frequency (MHz) = cycles * 1000 / interval_ns
  // conversion: cycles / (interval_ns / 1e9 / 1e6) = cycles * 1e9 / interval_ns / 1e6 = cycles * 1000 / interval_ns
  return static_cast<double>(cycles) * 1000.0 / static_cast<double>(interval_ns);
}

/**
 * @brief Calculate average event rate per second
 *
 * @param events Event count value
 * @param interval_ns Time interval in nanoseconds
 * @return Average event rate per second, or 0.0 if interval is 0
 */
inline double calculate_rate(uint64_t events, uint64_t interval_ns) {
  if (interval_ns == 0) {
    return 0.0;
  }
  // events per second = events * 1e9 / interval_ns
  return static_cast<double>(events) * 1e9 / static_cast<double>(interval_ns);
}

/**
 * @brief Calculate bandwidth in MiB/s
 *
 * @param total_bytes Total data in bytes
 * @param interval_ns Time interval in nanoseconds
 * @return Bandwidth in MiB/s, or 0.0 if interval is 0
 */
inline double calculate_bandwidth(uint64_t total_bytes, uint64_t interval_ns) {
  if (interval_ns == 0) {
    return 0.0;
  }
  // Bandwidth (MiB/s) = (total_bytes / (1024 * 1024)) / (interval_ns / 1e9)
  return static_cast<double>(total_bytes) / (1024.0 * 1024.0) / (static_cast<double>(interval_ns) / 1e9);
}

/**
 * @brief Calculate weighted average memory read latency in cycles
 *
 * Uses histogram buckets to compute weighted average:
 * - Bucket 0: 0-127 cycles (midpoint 63.5)
 * - Bucket 1: 128-191 cycles (midpoint 159.5)
 * - Bucket 2: 192-255 cycles (midpoint 223.5)
 * - Bucket 3: 256-319 cycles (midpoint 287.5)
 * - Bucket 4: 320-383 cycles (midpoint 351.5)
 * - Bucket 5: 384+ cycles (midpoint 416)
 *
 * @param lat0_127 Count of reads in 0-127 cycle range
 * @param lat128_191 Count of reads in 128-191 cycle range
 * @param lat192_255 Count of reads in 192-255 cycle range
 * @param lat256_319 Count of reads in 256-319 cycle range
 * @param lat320_383 Count of reads in 320-383 cycle range
 * @param lat384plus Count of reads in 384+ cycle range
 * @return Weighted average latency in cycles, or 0.0 if total count is 0
 */
inline double calculate_read_latency(uint64_t lat0_127, uint64_t lat128_191, uint64_t lat192_255,
                                     uint64_t lat256_319, uint64_t lat320_383, uint64_t lat384plus) {
  // Bucket midpoints
  constexpr double midpoints[] = {64, 160, 224, 288, 352, 416};
  uint64_t counts[] = {lat0_127, lat128_191, lat192_255, lat256_319, lat320_383, lat384plus};

  uint64_t total_count = 0;
  double weighted_sum = 0.0;

  for (int i = 0; i < 6; ++i) {
    total_count += counts[i];
    weighted_sum += static_cast<double>(counts[i]) * midpoints[i];
  }

  if (total_count == 0) {
    return 0.0;
  }

  return weighted_sum / static_cast<double>(total_count);
}