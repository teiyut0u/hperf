#include "hperf/config/hperf_config.hpp"

#include <iostream>

#include "hperf/config/detect_config.hpp"
#include "hperf/config/monitor_config.hpp"
#include "hperf/config/optimize_config.hpp"
#include "hperf/detect/detect.hpp"

// HperfConfig::HperfConfig() {
//   config_type = HELP;
//   config_data = nullptr;
// }
//
// HperfConfig::~HperfConfig() {
//   if (!config_type) {
//     return;
//   }
//   switch (config_type) {
//     case DETECT:
//       delete static_cast<DetectConfig*>(config_data);
//       break;
//     case MONOTOR:
//       delete static_cast<MonitorConfig*>(config_data);
//       break;
//     case OPTIMIZE:
//       delete static_cast<OptimizeConfig*>(config_data);
//       break;
//     default:
//       break;
//   }
// }

void HperfConfig::add_detect_cmd(CLI::App& app) {
  static DetectConfig detect_config;
  CLI::App* detect_cmd = app.add_subcommand("detect", "Detect target.");
  detect_cmd->callback([this]() {
    this->launch_config = []() {
      detect(detect_config);
    };
  });
  detect_cmd->add_option("target", detect_config.detect_target, "The target to detect.")->required()->option_text(" ");
}

void HperfConfig::add_optimize_cmd(CLI::App& app) {
  static OptimizeConfig optimize_config;
  CLI::App* optimize_cmd = app.add_subcommand("optimize");
  optimize_cmd->callback([this]() {
    this->launch_config = []() {
      // detect(detect_config);
      // todo
    };
  });
}

void HperfConfig::add_monitor_cmd(CLI::App& app) {
  static MonitorConfig monitor_config;
  CLI::App* monitor_cmd = app.add_subcommand("monitor");
  monitor_cmd->callback([this]() {
    this->launch_config = []() {
      // detect(detect_config);
      // todo
    };
  });
}

void HperfConfig::parse(int argc, char* argv[]) {
  CLI::App app("test app", "hperf");

  // add subcommands
  this->add_detect_cmd(app);
  this->add_optimize_cmd(app);
  this->add_monitor_cmd(app);

  // stderr usage if argv is not valid
  try {
    app.parse(argc, argv);
  } catch (CLI::ParseError e) {
    std::cerr << "Bad usage. Here's the help message:\n\n"
              << app.help();
  }

  // std::cout << "\n\n"
  //           << this->config_type << "\n\n";

  return;
}

// inline void HperfConfig::launch() {
//   switch (this->config_type) {
//     case DETECT:
//       // todo
//       break;
//     case OPTIMIZE:
//       // todo
//       break;
//     case MONOTOR:
//       // todo
//       break;
//     default:
//       break;
//   }
// }
