#include "hperf/config/config_launcher.hpp"

#include <iostream>

#include "hperf/config/cache_config.hpp"
#include "hperf/config/config_launcher.hpp"
#include "hperf/config/detect_config.hpp"
#include "hperf/config/monitor_config.hpp"
#include "hperf/config/optimize_config.hpp"
#include "hperf/detect/detect.hpp"
#include "hperf/util/hperf_cache.hpp"

void hperf::ConfigLauncher::parse(int argc, char* argv[]) {
  CLI::App app("test app", "hperf");

  // add subcommands
  this->add_detect_cmd(app);
  this->add_optimize_cmd(app);
  this->add_monitor_cmd(app);
  this->add_cache_cmd(app);

  // stderr usage if argv is not valid
  try {
    app.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    exit(app.exit(e));
  }

  return;
}

void hperf::ConfigLauncher::add_detect_cmd(CLI::App& app) {
  static hperf::DetectConfig detect_config;
  CLI::App* detect_cmd = app.add_subcommand("detect", "Detect target.");
  detect_cmd->callback([this]() {
    this->launch_config = []() {
      hperf::detect(detect_config);
    };
  });

  detect_cmd->add_option("target", detect_config.detect_target, "The target to detect.")->required()->option_text(" ");
}

void hperf::ConfigLauncher::add_optimize_cmd(CLI::App& app) {
  static hperf::OptimizeConfig optimize_config;
  CLI::App* optimize_cmd = app.add_subcommand("optimize");
  optimize_cmd->callback([this]() {
    this->launch_config = []() {
      // todo
    };
  });
}

void hperf::ConfigLauncher::add_monitor_cmd(CLI::App& app) {
  static hperf::MonitorConfig monitor_config;
  CLI::App* monitor_cmd = app.add_subcommand("monitor");
  monitor_cmd->callback([this]() {
    this->launch_config = []() {
      // todo
    };
  });
}

void hperf::ConfigLauncher::add_cache_cmd(CLI::App& app) {
  static hperf::CacheConfig cache_config;
  CLI::App* cache_cmd = app.add_subcommand("cache", "Manage cache.");
  CLI::App* cache_clean_cmd = cache_cmd->add_subcommand("clean", "Clean Specific cache. If not specified, all cache will be cleanned.");
  CLI::App* cache_list_cmd = cache_cmd->add_subcommand("list", "List all cache.");
  cache_clean_cmd->callback([this]() {
    this->launch_config = []() {
      if (cache_config.clean_target == "") {
        // clean all cache by default
        cache_config.clean_target = "all";
      }
      if (hperf::HperfCache::clean(cache_config.clean_target)) {
        std::cout << "Success to clean cache " << cache_config.clean_target << ".\n";
      } else {
        std::cout << "Failed to clean cache " << cache_config.clean_target << ".\n";
      }
    };
  });
  cache_list_cmd->callback([this]() {
    this->launch_config = []() {
      auto cache_list = hperf::HperfCache::list();
      if (cache_list.empty()) {
        std::cout << "There is no cache." << std::endl;
      } else {
        for (const auto& cache : cache_list) {
          std::cout << cache << std::endl;
        }
      }
    };
  });

  cache_clean_cmd->add_option("target", cache_config.clean_target, "The target cache to clean.")->option_text(" ");
}
