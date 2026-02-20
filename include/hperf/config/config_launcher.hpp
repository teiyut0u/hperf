/*
 * Copyright (c) 2026 [Your Company/Name]
 *
 * This file contains third-party software:
 *
 * CLI11 - Command line parser for C++11
 * Copyright (c) 2017-2026 University of Cincinnati
 * SPDX-License-Identifier: BSD-3-Clause
 * Source: https://github.com/CLIUtils/CLI11
 */

#ifndef HPERF_CONFIG_HPP
#define HPERF_CONFIG_HPP

#include <functional>

#include "CLI/CLI.hpp"

namespace hperf {

class ConfigLauncher {
 public:
  void parse(int argc, char* argv[]);
  inline void launch() {
    try {
      this->launch_config();
    } catch (const std::bad_function_call& e) {
      std::cerr << "failed to launch config\n";
    }
  }

 private:
  std::function<void(void)> launch_config;

  void add_detect_cmd(CLI::App& app);
  void add_optimize_cmd(CLI::App& app);
  void add_monitor_cmd(CLI::App& app);
  void add_cache_cmd(CLI::App& app);
};

}  // namespace hperf

#endif
