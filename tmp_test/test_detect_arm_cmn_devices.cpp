#include <cstdio>

#include "hperf/detect/arm_cmn_detector.hpp"

int main() {
  hperf::ArmCmnDetector arm_cmn_detector;
  unsigned int device_num = arm_cmn_detector.detect_devices();
  printf("find %u arm_cmn\n", device_num);
  return 0;
}
