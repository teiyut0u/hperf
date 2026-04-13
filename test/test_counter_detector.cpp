#include <gtest/gtest.h>

#include "hperf/counter_detector.h"

// Integration test: requires perf_event_open and must run on device.
// Verifies that CounterDetector does not crash and detects at least 1 counter.

TEST(CounterDetector, DetectReturnsPositiveCounters) {
  CounterDetector detector;
  detector.detect();
  EXPECT_GT(detector.get_detected_general_counter_num(), 0)
      << "Expected at least 1 programmable PMU counter on this device";
}

TEST(CounterDetector, SaveAndLoadDetectedResult) {
  CounterDetector detector;
  detector.detect();
  int original = detector.get_detected_general_counter_num();
  ASSERT_GT(original, 0);

  detector.save_detected_result();

  CounterDetector detector2;
  EXPECT_TRUE(detector2.load_detected_result());
  EXPECT_EQ(detector2.get_detected_general_counter_num(), original);
}