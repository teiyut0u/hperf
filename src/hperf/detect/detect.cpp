#include "hperf/detect/detect.hpp"

#include <iostream>

#include "hperf/detect/counter_detector.h"

void hperf::detect(const hperf::DetectConfig& detect_config) {
  if (detect_config.detect_target == "counter") {
    CounterDetector counter_detector;
    std::cout << "Detecting available programmable counters on each CPU ..." << std::endl;
    counter_detector.detect();
    counter_detector.print_result();
  } else {
    std::cout << "Invalid target! Valid targets are:\n...todo\n";
  }
}
