#ifndef HPERF_CONFIG_HPP
#define HPERF_CONFIG_HPP

#include <functional>

#include "CLI/CLI.hpp"

// enum CONFIG_TYPE {
//   DETECT,
//   OPTIMIZE,
//   MONOTOR,
//   HELP
// };

class HperfConfig {
 private:
  // CONFIG_TYPE config_type;
  // void* config_data;
  std::function<void(void)> launch_config;

  void add_detect_cmd(CLI::App& app);
  void add_optimize_cmd(CLI::App& app);
  void add_monitor_cmd(CLI::App& app);

 public:
  // HperfConfig();
  // ~HperfConfig();
  void parse(int argc, char* argv[]);
  // CONFIG_TYPE get_config_type() const;
  inline void launch() { this->launch_config(); }
};

#endif
