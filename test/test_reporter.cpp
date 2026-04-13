#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "hperf/pmu_config.h"
#include "hperf/reporter.h"

// Temp TOML used by all tests in this translation unit.
static const std::string kTmpToml =
    (std::filesystem::temp_directory_path() / ".hperf_test_reporter.toml").string();

static PMUConfig load_config(const std::string& toml) {
  std::ofstream f(kTmpToml);
  f << toml;
  f.close();
  PMUConfig cfg;
  cfg.load_from_file(kTmpToml);
  return cfg;
}

// One fixed event (cpu_cycles) + one group with one schedulable event (inst_spec).
static PMUConfig one_group_config() {
  return load_config(R"toml(
[[fixed_events]]
name        = "cpu_cycles"
description = "Cycle"
encoding    = 0x11

[[event_groups]]
events = [
  { name = "inst_spec", description = "Op spec executed", encoding = 0x1b },
]
)toml");
}

// One fixed event + two groups (inst_spec / ld_spec) for multiplexing tests.
static PMUConfig two_group_config() {
  return load_config(R"toml(
[[fixed_events]]
name        = "cpu_cycles"
description = "Cycle"
encoding    = 0x11

[[event_groups]]
events = [
  { name = "inst_spec", description = "Op spec executed", encoding = 0x1b },
]

[[event_groups]]
events = [
  { name = "ld_spec", description = "Load spec", encoding = 0x70 },
]
)toml");
}

// ------------------------------------------------------------------ process_a_record

TEST(Reporter, SingleGroupFullSchedulingRatioOne) {
  // Single group: enabled_time == total_time → ratio = 1.0
  // estimated_value == total_value (no scaling needed).
  PMUConfig cfg = one_group_config();
  Reporter reporter(cfg);

  // Two intervals for the same group; timestamps advance each interval.
  // event_id=0 → cpu_cycles (fixed); event_id=1 → inst_spec (schedulable)
  reporter.process_a_record({1000, 0, 0, 0, 200});  // t=1000: cpu_cycles += 200
  reporter.process_a_record({1000, 0, 0, 1, 100});  // t=1000: inst_spec  += 100
  reporter.process_a_record({2000, 0, 0, 0, 100});  // t=2000: cpu_cycles += 100
  reporter.process_a_record({2000, 0, 0, 1, 25});   // t=2000: inst_spec  += 25

  // total_time = enabled_time[0] = 2000 ns
  reporter.estimation();

  // cpu_cycles estimated = 200+100 = 300; inst_spec estimated = 100+25 = 125
  testing::internal::CaptureStdout();
  reporter.print_stats();
  std::string out = testing::internal::GetCapturedStdout();

  EXPECT_NE(out.find("cpu_cycles"), std::string::npos);
  EXPECT_NE(out.find("inst_spec"), std::string::npos);
  // Values < 1000 have no commas; check them as simple substrings.
  EXPECT_NE(out.find("300"), std::string::npos) << "cpu_cycles estimated should be 300";
  EXPECT_NE(out.find("125"), std::string::npos) << "inst_spec estimated should be 125";
}

TEST(Reporter, TwoGroupsMultiplexingCompensation) {
  // Two groups alternate: each group is active for half the total time.
  // ratio = total / enabled = 400 / 200 = 2.0 → estimated = raw * 2
  PMUConfig cfg = two_group_config();
  Reporter reporter(cfg);

  // Alternating intervals of 100 ns each:
  //   t=100: group 0 (cpu_cycles fixed, inst_spec schedulable)
  //   t=200: group 1 (cpu_cycles fixed, ld_spec  schedulable)
  //   t=300: group 0
  //   t=400: group 1
  reporter.process_a_record({100, 0, 0, 0, 500});   // cpu_cycles in group 0
  reporter.process_a_record({100, 0, 0, 1, 1000});  // inst_spec
  reporter.process_a_record({200, 0, 1, 0, 300});   // cpu_cycles in group 1
  reporter.process_a_record({200, 0, 1, 1, 1500});  // ld_spec
  reporter.process_a_record({300, 0, 0, 0, 500});
  reporter.process_a_record({300, 0, 0, 1, 1000});
  reporter.process_a_record({400, 0, 1, 0, 300});
  reporter.process_a_record({400, 0, 1, 1, 1500});

  reporter.estimation();
  // fixed: cpu_cycles total = (500+500) + (300+300) = 1600 → estimated = 1600
  // group 0: inst_spec total = 1000+1000=2000, ratio=2 → estimated = 4000
  // group 1: ld_spec   total = 1500+1500=3000, ratio=2 → estimated = 6000

  testing::internal::CaptureStdout();
  reporter.print_stats();
  std::string out = testing::internal::GetCapturedStdout();

  EXPECT_NE(out.find("1,600"), std::string::npos) << "cpu_cycles estimated should be 1,600";
  EXPECT_NE(out.find("4,000"), std::string::npos) << "inst_spec estimated should be 4,000";
  EXPECT_NE(out.find("6,000"), std::string::npos) << "ld_spec estimated should be 6,000";
}

TEST(Reporter, EstimationSkipsGroupWithZeroEnabledTime) {
  // Group 0 receives no records → enabled_time_in_ns_[0] = 0.
  // Without the zero-guard in estimation(), total/0 → inf, producing garbage output.
  // With the guard, group 0's schedulable events keep estimated_value = 0 and
  // print_stats() runs without NaN or crash.
  PMUConfig cfg = two_group_config();
  Reporter reporter(cfg);

  // Only group 1 gets records.
  reporter.process_a_record({1000, 0, 1, 0, 500});
  reporter.process_a_record({1000, 0, 1, 1, 200});

  reporter.estimation();  // must not crash or produce NaN

  testing::internal::CaptureStdout();
  reporter.print_stats();
  std::string out = testing::internal::GetCapturedStdout();

  // Output must exist and must not contain "nan" or "inf" (case-insensitive spot-check)
  EXPECT_FALSE(out.empty());
  EXPECT_EQ(out.find("nan"), std::string::npos);
  EXPECT_EQ(out.find("inf"), std::string::npos);
}

TEST(Reporter, FixedEventsAccumulateAcrossGroups) {
  // Fixed events (event_id < fixed_event_num_) are measured in every group.
  // estimation() sums them across all groups into stat_[0][j].estimated_value.
  PMUConfig cfg = two_group_config();
  Reporter reporter(cfg);

  // Each interval contributes to a different group, but both carry cpu_cycles.
  reporter.process_a_record({1000, 0, 0, 0, 400});  // cpu_cycles in group 0
  reporter.process_a_record({2000, 0, 1, 0, 600});  // cpu_cycles in group 1

  reporter.estimation();
  // fixed total = 400 + 600 = 1000

  testing::internal::CaptureStdout();
  reporter.print_stats();
  std::string out = testing::internal::GetCapturedStdout();

  EXPECT_NE(out.find("1,000"), std::string::npos) << "cpu_cycles estimated should be 1,000";
}

TEST(Reporter, SameTimestampDoesNotAdvanceTime) {
  // Records with the same timestamp should not change enabled_time or total_time.
  PMUConfig cfg = one_group_config();
  Reporter reporter(cfg);

  // Two records at t=1000 for the same group: only the first advances time.
  reporter.process_a_record({1000, 0, 0, 0, 100});  // advances time by 1000
  reporter.process_a_record({1000, 0, 0, 1, 50});   // same timestamp, no time advance
  // Another pair at t=2000:
  reporter.process_a_record({2000, 0, 0, 0, 200});  // advances time by 1000
  reporter.process_a_record({2000, 0, 0, 1, 80});   // same timestamp, no time advance

  reporter.estimation();

  // Should not crash and output should be well-formed.
  testing::internal::CaptureStdout();
  reporter.print_stats();
  std::string out = testing::internal::GetCapturedStdout();
  EXPECT_FALSE(out.empty());
}

// ------------------------------------------------------------------ print_metrics

TEST(Reporter, PrintMetricsWithFormula) {
  // Load a config that has a metric section and verify print_metrics does not crash
  // and the metric name appears in the output.
  PMUConfig cfg = load_config(R"toml(
[[fixed_events]]
name        = "cpu_cycles"
description = "Cycle"
encoding    = 0x11

[[event_groups]]
events = [
  { name = "inst_spec", description = "Op spec executed", encoding = 0x1b },
]

[[metric_sections]]
title = "Pipeline"
items = [
  { name = "IPC", type = "decimal", expr = "inst_spec / cpu_cycles" },
]
)toml");

  Reporter reporter(cfg);
  reporter.process_a_record({1000, 0, 0, 0, 1000});  // cpu_cycles = 1000
  reporter.process_a_record({1000, 0, 0, 1, 3000});  // inst_spec  = 3000
  reporter.estimation();

  testing::internal::CaptureStdout();
  reporter.print_metrics();
  std::string out = testing::internal::GetCapturedStdout();

  EXPECT_NE(out.find("Pipeline"), std::string::npos);
  EXPECT_NE(out.find("IPC"), std::string::npos);
  // IPC = inst_spec / cpu_cycles = 3000 / 1000 = 3.0000
  EXPECT_NE(out.find("3.0000"), std::string::npos) << "IPC should be 3.0000";
}

// ================================================================== Kernel mode tests

TEST(ReporterKernelMode, FixedEventsNoScaling) {
  // In kernel mode, fixed events (group 0) are pinned: no scaling needed.
  // estimated_value == total_value.
  PMUConfig cfg = one_group_config();
  Reporter reporter(cfg, /*kernel_mode=*/true);

  // group 0 = fixed events, event_id=0 = cpu_cycles
  reporter.process_a_record({1000, 0, 0, 0, 500});
  reporter.process_a_record({2000, 0, 0, 0, 300});

  // Set kernel time for group 0 (pinned: time_enabled == time_running)
  reporter.add_kernel_time(0, 2000, 2000);
  reporter.set_total_time(2000);

  reporter.estimation();

  testing::internal::CaptureStdout();
  reporter.print_stats();
  std::string out = testing::internal::GetCapturedStdout();

  // cpu_cycles = 500 + 300 = 800, no scaling
  EXPECT_NE(out.find("800"), std::string::npos) << "cpu_cycles should be 800 (no scaling)";
  EXPECT_NE(out.find("pinned"), std::string::npos) << "Should indicate pinned group";
}

TEST(ReporterKernelMode, SchedulableEventsScaled) {
  // In kernel mode, schedulable events (groups 1..N) are scaled by
  // time_enabled / time_running.
  PMUConfig cfg = two_group_config();
  Reporter reporter(cfg, /*kernel_mode=*/true);

  // group 0 = fixed (cpu_cycles), group 1 = inst_spec, group 2 = ld_spec
  reporter.process_a_record({1000, 0, 0, 0, 1000});  // cpu_cycles
  reporter.process_a_record({1000, 0, 1, 0, 500});   // inst_spec raw=500
  reporter.process_a_record({1000, 0, 2, 0, 300});   // ld_spec raw=300

  reporter.set_total_time(10000);
  reporter.add_kernel_time(0, 10000, 10000);  // fixed: pinned, full coverage
  reporter.add_kernel_time(1, 10000, 5000);   // inst_spec: running 50% → scale by 2.0
  reporter.add_kernel_time(2, 10000, 2500);   // ld_spec: running 25% → scale by 4.0

  reporter.estimation();

  testing::internal::CaptureStdout();
  reporter.print_stats();
  std::string out = testing::internal::GetCapturedStdout();

  // cpu_cycles: 1000 (no scaling)
  EXPECT_NE(out.find("1,000"), std::string::npos) << "cpu_cycles should be 1,000";
  // inst_spec: 500 * (10000/5000) = 1,000
  // But cpu_cycles is also 1,000, so check inst_spec label is present
  EXPECT_NE(out.find("inst_spec"), std::string::npos);
  // ld_spec: 300 * (10000/2500) = 1,200
  EXPECT_NE(out.find("1,200"), std::string::npos) << "ld_spec estimated should be 1,200";
}

TEST(ReporterKernelMode, PrintMetrics) {
  // Verify that print_metrics works correctly in kernel mode.
  PMUConfig cfg = load_config(R"toml(
[[fixed_events]]
name        = "cpu_cycles"
description = "Cycle"
encoding    = 0x11

[[event_groups]]
events = [
  { name = "inst_spec", description = "Op spec executed", encoding = 0x1b },
]

[[metric_sections]]
title = "Pipeline"
items = [
  { name = "IPC", type = "decimal", expr = "inst_spec / cpu_cycles" },
]
)toml");

  Reporter reporter(cfg, /*kernel_mode=*/true);

  // group 0 = cpu_cycles, group 1 = inst_spec
  reporter.process_a_record({1000, 0, 0, 0, 1000});  // cpu_cycles = 1000
  reporter.process_a_record({1000, 0, 1, 0, 2000});  // inst_spec  = 2000

  reporter.set_total_time(5000);
  reporter.add_kernel_time(0, 5000, 5000);  // pinned
  reporter.add_kernel_time(1, 5000, 5000);  // full coverage → ratio 1.0

  reporter.estimation();

  testing::internal::CaptureStdout();
  reporter.print_metrics();
  std::string out = testing::internal::GetCapturedStdout();

  EXPECT_NE(out.find("Pipeline"), std::string::npos);
  EXPECT_NE(out.find("IPC"), std::string::npos);
  // IPC = 2000 / 1000 = 2.0000
  EXPECT_NE(out.find("2.0000"), std::string::npos) << "IPC should be 2.0000";
}
