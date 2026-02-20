#include <cstdio>
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
  for (const auto& event : test_events) {
    auto res = hperf::parse_param_views(event.begin() + 4, event.begin() + event.size() - 1);
    printf("%s\n", event.c_str());
    for (const auto& pair : res) {
      std::cout << pair.first << " " << pair.second << std::endl;
    }
    std::cout << std::endl;
  }
  return 0;
}
