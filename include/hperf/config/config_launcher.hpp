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
  void add_clean_cmd(CLI::App& app);
};

}  // namespace hperf

#endif
