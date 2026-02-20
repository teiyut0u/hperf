#include <cstdio>
#include <filesystem>

#include "hperf/util/hperf_util.hpp"

int main() {
  auto attr_ptr = hperf::make_attr();
  auto err = hperf::add_field(std::filesystem::path("/sys/bus/event_source/devices/cpu/format/event"), 0xfff, *attr_ptr);
  if (err) {
    printf("%s\n", err.message().c_str());
  }
  printf("type: 0x%016x\nconfig: 0x%016llx\nconfig1: 0x%016llx\nconfig2: 0x%016llx\n", attr_ptr->type, attr_ptr->config, attr_ptr->config1, attr_ptr->config2);
  return 0;
}
