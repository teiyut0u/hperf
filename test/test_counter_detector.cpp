#include <iostream>

#include "hperf/counter_detector.h"

int main() {
  std::cout << "Test the number of available programmable counters" << std::endl;
  CounterDetector counter_detector;
  counter_detector.detect();
  counter_detector.print_result();
  return 0;
}