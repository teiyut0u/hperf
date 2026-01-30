#ifndef DETECT_CONFIG_HPP
#define DETECT_CONFIG_HPP

#include <string>

namespace hperf {

enum DetectTarget {
  COUNTER,
  ARM_CMN_SIZE,
  ARM_CMN_MC_POSITION
};

struct DetectConfig {
  std::string detect_target;
};

}  // namespace hperf

#endif
