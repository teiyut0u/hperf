/*
 * Copyright (c) 2023-2025 Arm Limited.
 *
 * SPDX-License-Identifier: MIT
 */

#include "gperf/visualizer.h"

#include <cstdint>

#include "hwcpipe/hwcpipe_counter.h"

std::string Visualizer::format_uint64(uint64_t value, int width) const {
  std::ostringstream oss;
  oss << std::setw(std::max(width, 7)) << value;
  return oss.str();
}

std::string Visualizer::format_rate(double rate, int width) const {
  std::ostringstream oss;
  int num_width = std::max(width, 9) - 3;  // Reserve 3 chars for " /s"
  oss << std::setw(num_width) << std::fixed << std::setprecision(0) << rate << " /s";
  return oss.str();
}

std::string Visualizer::format_mhz(double mhz) const {
  std::ostringstream oss;
  oss << std::setw(8) << std::fixed << std::setprecision(3) << mhz << " MHz";
  return oss.str();
}

std::string Visualizer::format_util(double util) const {
  std::ostringstream oss;
  oss << std::setw(5) << std::fixed << std::setprecision(2) << util << " %";
  return oss.str();
}

std::string Visualizer::format_bandwidth(double mib_s) const {
  std::ostringstream oss;
  oss << std::setw(9) << std::fixed << std::setprecision(3) << mib_s << " MiB/s";
  return oss.str();
}

std::string Visualizer::format_latency(double latency) const {
  std::ostringstream oss;
  oss << std::setw(8) << std::fixed << std::setprecision(2) << latency << " Cycles";
  return oss.str();
}

CounterValue Visualizer::get_value(const std::unordered_map<hwcpipe_counter, CounterValue> &data,
                                   hwcpipe_counter key, const CounterValue &default_val) const {
  auto it = data.find(key);
  return (it != data.end()) ? it->second : default_val;
}

void Visualizer::visualize(const std::unordered_map<hwcpipe_counter, CounterValue> &data,
                           uint64_t interval) {
  // Clear console
  std::cout << "\033[2J\033[H";

  // Extract commonly used values
  uint64_t gpu_active_cycles = get_value(data, MaliGPUActiveCy).as_uint64();
  double gpu_active_cycles_mhz = calculate_mhz(gpu_active_cycles, interval);

  uint64_t compute_jobs = get_value(data, MaliCompQueueJob).as_uint64();
  uint64_t binning_jobs = get_value(data, MaliBinningQueueJob).as_uint64();
  uint64_t main_jobs = get_value(data, MaliMainQueueJob).as_uint64();

  double compute_jobs_rate = calculate_rate(compute_jobs, interval);
  double binning_jobs_rate = calculate_rate(binning_jobs, interval);
  double main_jobs_rate = calculate_rate(main_jobs, interval);

  uint64_t compute_active_cycles = get_value(data, MaliCompQueueActiveCy).as_uint64();
  uint64_t binning_active_cycles = get_value(data, MaliBinningQueueActiveCy).as_uint64();
  uint64_t main_active_cycles = get_value(data, MaliMainQueueActiveCy).as_uint64();

  double compute_active_cycles_mhz = calculate_mhz(compute_active_cycles, interval);
  double binning_active_cycles_mhz = calculate_mhz(binning_active_cycles, interval);
  double main_active_cycles_mhz = calculate_mhz(main_active_cycles, interval);

  double compute_util = get_value(data, MaliCompQueueUtil).as_double();
  double binning_util = get_value(data, MaliBinningQueueUtil).as_double();
  double main_util = get_value(data, MaliMainQueueUtil).as_double();

  uint64_t compute_tasks = get_value(data, MaliCompQueueTask).as_uint64();
  uint64_t binning_tasks = get_value(data, MaliBinningQueueTask).as_uint64();
  uint64_t main_tasks = get_value(data, MaliMainQueueTask).as_uint64();

  double compute_tasks_rate = calculate_rate(compute_tasks, interval);
  double binning_tasks_rate = calculate_rate(binning_tasks, interval);
  double main_tasks_rate = calculate_rate(main_tasks, interval);

  uint64_t ext_read_bytes = get_value(data, MaliExtBusRdBy).as_uint64();
  uint64_t ext_write_bytes = get_value(data, MaliExtBusWrBy).as_uint64();

  double ext_read_bandwidth = calculate_bandwidth(ext_read_bytes, interval);
  double ext_write_bandwidth = calculate_bandwidth(ext_write_bytes, interval);

  // Get external bus read latency histogram buckets
  uint64_t ext_read_lat0_127 = get_value(data, MaliExtBusRdLat0).as_uint64();
  uint64_t ext_read_lat128_191 = get_value(data, MaliExtBusRdLat128).as_uint64();
  uint64_t ext_read_lat192_255 = get_value(data, MaliExtBusRdLat192).as_uint64();
  uint64_t ext_read_lat256_319 = get_value(data, MaliExtBusRdLat256).as_uint64();
  uint64_t ext_read_lat320_383 = get_value(data, MaliExtBusRdLat320).as_uint64();
  uint64_t ext_read_lat384plus = get_value(data, MaliExtBusRdLat384).as_uint64();

  // Calculate weighted average read latency
  double ext_read_latency = calculate_read_latency(ext_read_lat0_127,
                                                   ext_read_lat128_191,
                                                   ext_read_lat192_255,
                                                   ext_read_lat256_319,
                                                   ext_read_lat320_383,
                                                   ext_read_lat384plus);

  // Print visualization based on layout.txt
  std::cout << "                  GPU Active Cycles  [ " << format_uint64(gpu_active_cycles, 12) << " ]\n";
  std::cout << "                                     ( " << format_mhz(gpu_active_cycles_mhz) << " )\n";
  std::cout << "                                             │\n";
  std::cout << "                        ┌────────────────────┼─────────────────────┐\n";
  std::cout << "                        │                    │                     │\n";
  std::cout << "          Jobs     [ " << format_uint64(compute_jobs, 7) << " ]          [ "
            << format_uint64(binning_jobs, 7) << " ]           [ "
            << format_uint64(main_jobs, 7) << " ]\n";
  std::cout << "                  ( " << format_rate(compute_jobs_rate) << " )        ( "
            << format_rate(binning_jobs_rate) << " )         ( "
            << format_rate(main_jobs_rate) << " )\n";
  std::cout << "                        │                    │                     │\n";
  std::cout << "   Work Queues    Compute Queue      Binning Phase Queue    Main Phase Queue\n";
  std::cout << " Active Cycles   [ " << format_uint64(compute_active_cycles, 12) << " ]     [ " << format_uint64(binning_active_cycles, 12) << " ]      [ " << format_uint64(main_active_cycles, 12) << " ]\n";
  std::cout << "                 ( " << format_mhz(compute_active_cycles_mhz) << " )     ( "
            << format_mhz(binning_active_cycles_mhz) << " )      ( "
            << format_mhz(main_active_cycles_mhz) << " )\n";
  std::cout << "                        │                    │                     │\n";
  std::cout << "   Utilization     [ " << format_util(compute_util) << " ]          [ " << format_util(binning_util) << " ]           [ " << format_util(main_util) << " ]\n";
  std::cout << "                        │                    │                     │\n";
  std::cout << "         Tasks    [ " << format_uint64(compute_tasks, 9) << " ]        [ "
            << format_uint64(binning_tasks, 9) << " ]         [ "
            << format_uint64(main_tasks, 9) << " ]\n";
  std::cout << "                 ( " << format_rate(compute_tasks_rate, 11) << " )      ( "
            << format_rate(binning_tasks_rate, 11) << " )       ( "
            << format_rate(main_tasks_rate, 11) << " )\n";
  std::cout << "                        │                    │                     │\n";
  std::cout << "                ┌───────┴────────────────────┴─────────────────────┴───────┐\n";
  std::cout << "                │                       Shader Cores                       │\n";
  std::cout << "                └───────────────┬────────────────────────┬─────────────────┘\n";
  std::cout << "                                │                        │\n";
  std::cout << "                        External Read Port      External Write Port\n";
  std::cout << "                                │                        │\n";
  std::cout << "     Bandwidth          [ " << format_bandwidth(ext_read_bandwidth) << " ]     [ " << format_bandwidth(ext_write_bandwidth) << " ]\n";
  std::cout << "       Latency          [ " << format_latency(ext_read_latency) << " ]\n";
  std::cout << std::flush;
}
