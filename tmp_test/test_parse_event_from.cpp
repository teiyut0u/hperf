#include <array>
#include <cassert>
#include <cstdio>
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
    // invalid, will error
    "cpu/cache-misses,edge=w,inv=0x1/",
};

std::vector<std::array<uint64_t, 4>> test_answer = {
    {{0x0000000000000004, 0x00000000000000c2, 0x0000000000000000, 0x0000000000000000}},
    {{0x0000000000000004, 0x00000000000000c3, 0x0000000000000000, 0x0000000000000000}},
    {{0x0000000000000004, 0x0000000000000964, 0x0000000000000000, 0x0000000000000000}},
    {{0x0000000000000004, 0x000000000000ff60, 0x0000000000000000, 0x0000000000000000}},
    {{0x0000000000000004, 0x0000000000000076, 0x0000000000000000, 0x0000000000000000}},
    {{0x0000000000000004, 0x00000000000000c0, 0x0000000000000000, 0x0000000000000000}},
    {{0x0000000000000004, 0x0000000100000120, 0x0000000000000000, 0x0000000000000000}},
    {{0x0000000000000004, 0x00000000000000a9, 0x0000000000000000, 0x0000000000000000}},
    // add param
    {{0x0000000000000004, 0x00000000660000c2, 0x0000000000000000, 0x0000000000000000}},
    {{0x0000000000000004, 0x00000000000400c3, 0x0000000000000000, 0x0000000000000000}},
    {{0x0000000000000004, 0x0000000000840964, 0x0000000000000000, 0x0000000000000000}},
    {{}},
};

int main() {
  for (auto i = 0; i < test_events.size(); ++i) {
    auto attr_ptr = hperf::make_attr();
    auto err = hperf::parse_event_from(test_events[i], *attr_ptr);
    if (err) {
      printf("Test point %d error: %s.\n\tIgnore it if it's designed to error\n", i, err.message().c_str());
      continue;
    }
    if (attr_ptr->type != test_answer[i][0] ||
        attr_ptr->config != test_answer[i][1] ||
        attr_ptr->config1 != test_answer[i][2] ||
        attr_ptr->config2 != test_answer[i][3]) {
      printf("Test point %d failed:\n%s:\ntype: 0x%016x\nconfig: 0x%016llx\nconfig1: 0x%016llx\nconfig2: 0x%016llx\n", i, test_events[i].c_str(), attr_ptr->type, attr_ptr->config, attr_ptr->config1, attr_ptr->config2);
    }
  }
  return 0;
}
