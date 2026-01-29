#include "hperf/config/hperf_config.hpp"

int main(int argc, char* argv[]) {
  HperfConfig hperf_config;
  hperf_config.parse(argc, argv);
  hperf_config.launch();
  return 0;
}
