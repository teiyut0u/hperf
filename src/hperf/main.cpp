#include "hperf/config/config_launcher.hpp"

int main(int argc, char* argv[]) {
  hperf::ConfigLauncher config_launcher;
  config_launcher.parse(argc, argv);
  config_launcher.launch();
  return 0;
}
