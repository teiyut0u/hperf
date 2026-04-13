/*
 * Copyright (c) 2023-2025 Arm Limited.
 *
 * SPDX-License-Identifier: MIT
 */

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <device/product_id.hpp>
#include <fstream>
#include <hwcpipe/counter_database.hpp>
#include <hwcpipe/gpu.hpp>
#include <hwcpipe/sampler.hpp>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gperf/record.h"
#include "gperf/visualizer.h"
#include "hwcpipe/hwcpipe_counter.h"

/**
 * @brief Helper function to print help message
 *
 * @param prog_name Program name, i.e., 'gperf'
 */
void print_help(const char *prog_name) {
  std::cout << "Usage: " << prog_name << " [options]\n\n"
            << "Options:\n"
            << "  -h, --help          Show this help message and exit.\n"
            << "  --print-info        Print GPU static information and supported counters, then exit.\n"
            << "  -i, --interval <ms> Sampling interval in milliseconds (default: 1000).\n"
            << "  -d, --duration <s>  Total sampling duration in seconds (default: 20).\n"
            << "  -o, --output <file> Output results to a CSV file.\n"
            << '\n';
}

/**
 * @brief Get the string of GPU family name
 *
 * @param f The enumaration variable defined in hwcpipe::device::gpu_family
 * @return const char* String of the GPU family name
 */
const char *get_product_family_name(hwcpipe::device::gpu_family f) {
  using gpu_family = hwcpipe::device::gpu_family;

  switch (f) {
    case gpu_family::bifrost:
      return "Bifrost";
    case gpu_family::midgard:
      return "Midgard";
    case gpu_family::valhall:
      return "Valhall";
    case gpu_family::fifthgen:
      return "Arm 5th Gen";
    default:
      return "Unknown";
  }
}

/**
 * @brief Get the string of GPU product name, only for Arm 5th Gen family products
 *
 * @param id The enumaration variable defined in hwcpipe::device::product_id
 * @return const char* String of the GPU product name
 */
const char *get_product_id_name(hwcpipe::device::product_id id) {
  using product_id = hwcpipe::device::product_id;

  switch (id) {
    case product_id::g720:
      return "G720";
    case product_id::g620:
      return "G620";
    case product_id::g725:
      return "G725";
    case product_id::g625:
      return "G625";
    case product_id::g1_ultra:
      return "G1 Ultra";
    case product_id::g1_premium:
      return "G1 Premium";
    case product_id::g1_pro:
      return "G1 Pro";
    default:
      return "Other product";
  }
}

/**
 * @brief Print GPU static info such as family, ID, the number of Shader Cores, and list all supported GPU performance event.
 *
 */
void print_gpu_info() {
  // Probe device 0 (i.e. /dev/mali0)
  auto gpu = hwcpipe::gpu(0);
  if (!gpu) {
    std::cout << "Mali GPU device 0 is missing."
              << "Please check if the device file /dev/mali0 exists." << '\n';
    return;
  }

  // Print static info
  std::cout << "------------------------------------------------------------\n"
            << " GPU Device Information:\n"
            << "------------------------------------------------------------\n";
  std::cout << "  Product Family:              " << get_product_family_name(gpu.get_gpu_family()) << '\n';
  std::cout << "  Product ID:                  " << get_product_id_name(gpu.get_product_id()) << '\n';
  std::cout << "  Number of Shader Cores:      " << gpu.num_shader_cores() << '\n';
  std::cout << "  Number of Execution Engines: " << gpu.num_execution_engines() << '\n';
  std::cout << "  Tile Size (pixels):          " << gpu.get_constants().tile_size << '\n';
  std::cout << "  Warp Width:                  " << gpu.get_constants().warp_width << '\n';
  std::cout << "  Number of L2 Cache Slices:   " << gpu.get_constants().num_l2_slices << '\n';
  std::cout << "  L2 Cache Slice Size (bytes): " << gpu.get_constants().l2_slice_size << '\n';
  std::cout << "  Bus Width (bits):            " << gpu.bus_width() << '\n';

  // Print the counters that it supports
  auto counter_db = hwcpipe::counter_database{};
  hwcpipe::counter_metadata meta;

  std::cout << '\n'
            << "------------------------------------------------------------\n"
            << " Supported GPU Hardware Events:\n"
            << " Event ID - Event Name (Unit) \n"
            << "------------------------------------------------------------\n";

  // Sort events by ID frist
  auto counters = counter_db.counters_for_gpu(gpu);
  std::vector<hwcpipe_counter> counters_vec(counters.begin(), counters.end());
  std::sort(counters_vec.begin(), counters_vec.end(),
            [](hwcpipe_counter a, hwcpipe_counter b) {
              return static_cast<std::underlying_type_t<hwcpipe_counter>>(a) <
                     static_cast<std::underlying_type_t<hwcpipe_counter>>(b);
            });

  for (hwcpipe_counter counter : counters_vec) {
    auto ec = counter_db.describe_counter(counter, meta);
    if (ec) {
      assert(false);
    }
    std::cout << std::setw(4) << counter << " - " << meta.name << " (" << meta.units << ")" << '\n';
  }
}

/**
 * @brief Print the event count or metric
 *
 * @param os An output stream, it can be std::cout or a std::ofstream object
 * @param sample A object holds the sampled information from a counter at a specific timestamp
 */
void print_sample_value(std::ostream &os, const hwcpipe::counter_sample &sample) {
  switch (sample.type) {
    case hwcpipe::counter_sample::type::uint64:
      os << sample.value.uint64;
      return;
    case hwcpipe::counter_sample::type::float64:
      os << sample.value.float64;
      return;
    default:
      os << "Unknown";
      return;
  }
}

int main(int argc, char *argv[]) {
  // Command-line arguments
  bool print_info_flag = false;
  long interval_ms = 1000;
  long duration_s = 20;
  std::string output_file;

  // Parse command-line arguments
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      print_help(argv[0]);
      return 0;
    } else if (arg == "--print-info") {
      print_info_flag = true;
    } else if (arg == "-i" || arg == "--interval") {
      if (i + 1 < argc) {
        interval_ms = std::stol(argv[++i]);
      }
    } else if (arg == "-d" || arg == "--duration") {
      if (i + 1 < argc) {
        duration_s = std::stol(argv[++i]);
      }
    } else if (arg == "-o" || arg == "--output") {
      if (i + 1 < argc) {
        output_file = argv[++i];
      }
    }
  }

  if (print_info_flag) {
    print_gpu_info();
    return 0;
  }

  // Probe device 0 (i.e. /dev/mali0)
  auto gpu = hwcpipe::gpu(0);
  if (!gpu) {
    std::cout << "Mali GPU device 0 is missing."
              << "Please check if the device file /dev/mali0 exists." << '\n';
    return -1;
  }

  std::error_code ec;
  auto config = hwcpipe::sampler_config(gpu);

  // GPU counters to be measured
  std::vector<std::pair<hwcpipe_counter, std::string>> counters = {
      std::make_pair(MaliGPUActiveCy, "GPU Active Cycles"),

      std::make_pair(MaliBinningQueueJob, "Binning Phase Jobs"),
      std::make_pair(MaliMainQueueJob, "Main Phase Jobs"),
      std::make_pair(MaliCompQueueJob, "Compute Jobs"),

      std::make_pair(MaliBinningQueueTask, "Binning Phase Tasks"),
      std::make_pair(MaliMainQueueTask, "Main Phase Tasks"),
      std::make_pair(MaliCompQueueTask, "Compute Tasks"),

      std::make_pair(MaliBinningQueueActiveCy, "Binning Phase Queue Active Cycles"),
      std::make_pair(MaliMainQueueActiveCy, "Main Phase Queue Active Cycles"),
      std::make_pair(MaliCompQueueActiveCy, "Compute Queue Active Cycles"),

      std::make_pair(MaliBinningQueueUtil, "Binning Phase Queue Utilization"),
      std::make_pair(MaliMainQueueUtil, "Main Phase Queue Utilization"),
      std::make_pair(MaliCompQueueUtil, "Compute Queue Utilization"),

      std::make_pair(MaliExtBusRdBy, "Output external read bytes"),
      std::make_pair(MaliExtBusWrBy, "Output external write bytes"),

      std::make_pair(MaliExtBusRdLat0, "Output external read latency 0-127 cycles"),
      std::make_pair(MaliExtBusRdLat128, "Output external read latency 128-191 cycles"),
      std::make_pair(MaliExtBusRdLat192, "Output external read latency 192-255 cycles"),
      std::make_pair(MaliExtBusRdLat256, "Output external read latency 256-319 cycles"),
      std::make_pair(MaliExtBusRdLat320, "Output external read latency 320-383 cycles"),
      std::make_pair(MaliExtBusRdLat384, "Output external read latency 384+ cycles")};

  for (auto counter : counters) {
    ec = config.add_counter(counter.first);
    if (ec) {
      std::cout << counter.second << " counter does not supported by this GPU." << '\n';
      return -1;
    }
  }

  // Prepare output stream
  std::ofstream file_stream;
  if (!output_file.empty()) {
    file_stream.open(output_file);
    if (!file_stream.is_open()) {
      std::cerr << "Error: Could not open output file " << output_file << '\n';
      return -1;
    }
  }

  auto sampler = hwcpipe::sampler<>(config);
  hwcpipe::counter_sample sample;

  Visualizer visualizer;
  Record record;
  uint64_t duration_ns = static_cast<uint64_t>(duration_s) * 1000000000ULL;

  ec = sampler.start_sampling();
  if (ec) {
    std::cout << ec.message() << '\n';
    return -1;
  }

  // Get the start timestamp
  ec = sampler.sample_now();
  uint64_t first_timestamp;
  if (ec) {
    std::cout << ec.message() << '\n';
    return -1;
  } else {
    ec = sampler.get_counter_value(counters[0].first, sample);
    if (!ec) {
      first_timestamp = sample.timestamp;
    } else {
      return -1;
    }
  }

  std::cout << "Collecting GPU performance data ..." << '\n';
  // Clear console
  std::cout << "\033[2J\033[H";

  // Write CSV header if output file is specified
  if (!output_file.empty()) {
    file_stream << "timestamp,event,value\n";
  }

  uint64_t elapsed_time = 0;
  uint64_t real_interval;

  while (elapsed_time < duration_ns) {
    std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    // Sample at this timestamp
    ec = sampler.sample_now();

    if (ec) {
      std::cout << ec.message() << '\n';
      continue;
    }

    // Collect all counter values into a map for visualization
    std::unordered_map<hwcpipe_counter, CounterValue> perf_data;
    uint64_t interval = 0;

    // Get the timestamp and update the elapsed time
    ec = sampler.get_counter_value(counters[0].first, sample);

    if (!ec) {
      uint64_t old_elapsed_time = elapsed_time;
      elapsed_time = sample.timestamp - first_timestamp;
      interval = elapsed_time - old_elapsed_time;
    }

    // Fetch counter values
    for (auto counter : counters) {
      ec = sampler.get_counter_value(counter.first, sample);
      if (!ec) {
        // Store data in map for visualization, preserving original type
        if (sample.type == hwcpipe::counter_sample::type::uint64) {
          perf_data[counter.first] = CounterValue(sample.value.uint64);
        } else if (sample.type == hwcpipe::counter_sample::type::float64) {
          perf_data[counter.first] = CounterValue(sample.value.float64);
        }

        // Write to CSV if output file is specified
        if (!output_file.empty()) {
          file_stream << elapsed_time << "," << counter.second << ",";
          print_sample_value(file_stream, sample);
          file_stream << "\n";
        }
      }
    }

    // Record accumulated performance data
    record.record(perf_data);

    // Visualize performance data
    if (!perf_data.empty()) {
      visualizer.visualize(perf_data, interval);
      std::cout << "Interval  [ " << std::setw(12) << interval << " ns ]\n";
      std::cout << "Duration  [ " << std::setw(12) << elapsed_time << " ns ]\n";
      std::cout << std::flush;
    }
  }  // end while

  ec = sampler.stop_sampling();
  if (ec) {
    std::cout << ec.message() << '\n';
  }

  // Visualize accumulated data at the end
  const auto &accumulated = record.get_data();
  if (!accumulated.empty()) {
    visualizer.visualize(accumulated, elapsed_time);
    std::cout << "Performance Summary \n";
    std::cout << "Duration  [ " << std::setw(12) << elapsed_time << " ns ]\n";
    std::cout << std::flush;
  }

  std::cout << "Data collection finished" << '\n';

  if (file_stream.is_open()) {
    std::cout << "Performance data has been saved in " << output_file << '\n';
    file_stream.close();
  }

  return 0;
}
