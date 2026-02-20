#include <cstdio>
#include <filesystem>
#include <iostream>
#include <vector>

#include "hperf/util/hperf_util.hpp"

std::vector<std::string> test_events = {
    "cpu/branch-instructions/",
    "cpu/branch-misses/",
    "cpu/cache-misses/",
    "cpu/cache-references/",
    "cpu/cpu-cycles/",
    "cpu/instructions/",
    "cpu/ref-cycles/",
    "cpu/stalled-cycles-frontend/",
    // add param
    "cpu/branch-instructions,cmask=0x66/",
    "cpu/branch-misses,edge=0x1/",
    "cpu/cache-misses,edge=1,inv=0x1/",
    "cpu/cache-misses,edge=w,inv=0x1/",
};

int main() {
  std::filesystem::path event_path("/sys/bus/event_source/devices/cpu/events/ref-cycles");
  auto attr_ptr = hperf::make_empty_attr();
  auto err = hperf::add_event(event_path, *attr_ptr);
  if (err) {
    printf("%s\n", err.message().c_str());
  }
  printf("type: 0x%016x\nconfig: 0x%016llx\nconfig1: 0x%016llx\nconfig2: 0x%016llx\n", attr_ptr->type, attr_ptr->config, attr_ptr->config1, attr_ptr->config2);
  return 0;
}
