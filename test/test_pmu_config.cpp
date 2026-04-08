#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "hperf/hperf_error.h"
#include "hperf/pmu_config.h"

// Temp file path used by all tests in this translation unit.
static const std::string kTmpToml =
    (std::filesystem::temp_directory_path() / ".hperf_test_pmu_config.toml").string();

static void write_file(const std::string& path, const std::string& content) {
  std::ofstream f(path);
  f << content;
}

// Minimal TOML that satisfies load_from_file: 1 fixed event + 1 group of 1 event.
static std::string minimal_valid_toml() {
  return R"toml(
[[fixed_events]]
name        = "cpu_cycles"
description = "Cycle"
encoding    = 0x11

[[event_groups]]
events = [
  { name = "inst_spec", description = "Op speculatively executed", encoding = 0x1b },
]
)toml";
}

// ------------------------------------------------------------------ load_from_file: valid inputs

TEST(PMUConfig, LoadValidMinimalConfig) {
  write_file(kTmpToml, minimal_valid_toml());
  PMUConfig cfg;
  ASSERT_NO_THROW(cfg.load_from_file(kTmpToml));
  EXPECT_TRUE(cfg.is_valid());

  ASSERT_EQ(cfg.get_fixed_events().size(), 1u);
  EXPECT_EQ(cfg.get_fixed_events()[0].name, "cpu_cycles");
  EXPECT_EQ(cfg.get_fixed_events()[0].encoding, 0x11u);

  ASSERT_EQ(cfg.get_event_group_num(), 1u);
  ASSERT_EQ(cfg.get_event_group_by_idx(0).size(), 1u);
  EXPECT_EQ(cfg.get_event_group_by_idx(0)[0].name, "inst_spec");
  EXPECT_EQ(cfg.get_event_group_by_idx(0)[0].encoding, 0x1bu);
}

TEST(PMUConfig, LoadMultipleFixedEventsAndGroups) {
  write_file(kTmpToml, R"toml(
[[fixed_events]]
name = "cpu_cycles"
description = "Cycle"
encoding = 0x11

[[fixed_events]]
name = "inst_retired"
description = "Retired instruction"
encoding = 0x08

[[event_groups]]
events = [
  { name = "ld_spec", description = "Load spec", encoding = 0x70 },
  { name = "st_spec", description = "Store spec", encoding = 0x71 },
]

[[event_groups]]
events = [
  { name = "br_mis_pred_retired", description = "Branch mispred", encoding = 0x22 },
]
)toml");
  PMUConfig cfg;
  ASSERT_NO_THROW(cfg.load_from_file(kTmpToml));

  EXPECT_EQ(cfg.get_fixed_events().size(), 2u);
  EXPECT_EQ(cfg.get_event_group_num(), 2u);
  EXPECT_EQ(cfg.get_event_group_by_idx(0).size(), 2u);
  EXPECT_EQ(cfg.get_event_group_by_idx(1).size(), 1u);
}

TEST(PMUConfig, MetricSectionsOptional) {
  // No metric_sections in the file → still loads successfully with empty sections
  write_file(kTmpToml, minimal_valid_toml());
  PMUConfig cfg;
  ASSERT_NO_THROW(cfg.load_from_file(kTmpToml));
  EXPECT_EQ(cfg.get_metric_sections().size(), 0u);
}

TEST(PMUConfig, LoadWithMetricSections) {
  std::string toml = minimal_valid_toml() + R"toml(
[[metric_sections]]
title = "Pipeline"
items = [
  { name = "IPC",    type = "decimal",    expr = "inst_spec / cpu_cycles" },
  { name = "CPU util", type = "percentage", expr = "cpu_cycles / total_time_ns" },
]

[[metric_sections]]
title = "Cache"
items = [
  { name = "L1D MPKI", type = "decimal", expr = "l1d_cache_refill * 1000 / inst_spec" },
]
)toml";
  write_file(kTmpToml, toml);
  PMUConfig cfg;
  ASSERT_NO_THROW(cfg.load_from_file(kTmpToml));

  ASSERT_EQ(cfg.get_metric_sections().size(), 2u);

  EXPECT_EQ(cfg.get_metric_sections()[0].title, "Pipeline");
  ASSERT_EQ(cfg.get_metric_sections()[0].items.size(), 2u);
  EXPECT_EQ(cfg.get_metric_sections()[0].items[0].name, "IPC");
  EXPECT_EQ(cfg.get_metric_sections()[0].items[0].type, "decimal");
  EXPECT_EQ(cfg.get_metric_sections()[0].items[0].expr, "inst_spec / cpu_cycles");

  EXPECT_EQ(cfg.get_metric_sections()[1].title, "Cache");
  ASSERT_EQ(cfg.get_metric_sections()[1].items.size(), 1u);
  EXPECT_EQ(cfg.get_metric_sections()[1].items[0].name, "L1D MPKI");
}

// ------------------------------------------------------------------ load_from_file: error cases

TEST(PMUConfig, NonexistentFileReturnsFalse) {
  PMUConfig cfg;
  EXPECT_THROW(cfg.load_from_file("/nonexistent/path/config.toml"), HperfError);
}

TEST(PMUConfig, MissingFixedEventsReturnsFalse) {
  write_file(kTmpToml, R"toml(
[[event_groups]]
events = [
  { name = "inst_spec", description = "Op spec", encoding = 0x1b },
]
)toml");
  PMUConfig cfg;
  EXPECT_THROW(cfg.load_from_file(kTmpToml), HperfError);
}

TEST(PMUConfig, MissingEventGroupsReturnsFalse) {
  write_file(kTmpToml, R"toml(
[[fixed_events]]
name        = "cpu_cycles"
description = "Cycle"
encoding    = 0x11
)toml");
  PMUConfig cfg;
  EXPECT_THROW(cfg.load_from_file(kTmpToml), HperfError);
}

TEST(PMUConfig, MissingFixedEventDescriptionReturnsFalse) {
  write_file(kTmpToml, R"toml(
[[fixed_events]]
name     = "cpu_cycles"
encoding = 0x11

[[event_groups]]
events = [
  { name = "inst_spec", description = "Op spec", encoding = 0x1b },
]
)toml");
  PMUConfig cfg;
  EXPECT_THROW(cfg.load_from_file(kTmpToml), HperfError);
}

TEST(PMUConfig, MissingSchedulableEventEncodingReturnsFalse) {
  write_file(kTmpToml, R"toml(
[[fixed_events]]
name        = "cpu_cycles"
description = "Cycle"
encoding    = 0x11

[[event_groups]]
events = [
  { name = "inst_spec", description = "Op spec" },
]
)toml");
  PMUConfig cfg;
  EXPECT_THROW(cfg.load_from_file(kTmpToml), HperfError);
}

TEST(PMUConfig, MissingMetricItemTypeReturnsFalse) {
  write_file(kTmpToml, minimal_valid_toml() + R"toml(
[[metric_sections]]
title = "Pipeline"
items = [
  { name = "IPC", expr = "inst_spec / cpu_cycles" },
]
)toml");
  PMUConfig cfg;
  EXPECT_THROW(cfg.load_from_file(kTmpToml), HperfError);
}

// ------------------------------------------------------------------ is_valid

TEST(PMUConfig, IsValidFalseBeforeLoad) {
  PMUConfig cfg;
  EXPECT_FALSE(cfg.is_valid());
}

TEST(PMUConfig, IsValidTrueAfterSuccessfulLoad) {
  write_file(kTmpToml, minimal_valid_toml());
  PMUConfig cfg;
  ASSERT_NO_THROW(cfg.load_from_file(kTmpToml));
  EXPECT_TRUE(cfg.is_valid());
}

// ------------------------------------------------------------------ get_pmu_event

TEST(PMUConfig, GetPmuEventFixedByIndex) {
  write_file(kTmpToml, minimal_valid_toml());
  PMUConfig cfg;
  ASSERT_NO_THROW(cfg.load_from_file(kTmpToml));
  // event_idx=0 < fixed_events_.size()=1 → returns fixed_events_[0]
  EXPECT_EQ(cfg.get_pmu_event(0, 0).name, "cpu_cycles");
}

TEST(PMUConfig, GetPmuEventSchedulableByIndex) {
  write_file(kTmpToml, minimal_valid_toml());
  PMUConfig cfg;
  ASSERT_NO_THROW(cfg.load_from_file(kTmpToml));
  // event_idx=1 >= fixed_events_.size()=1 → returns event_groups_[0][0]
  EXPECT_EQ(cfg.get_pmu_event(0, 1).name, "inst_spec");
}

TEST(PMUConfig, GetPmuEventOutOfBoundsGroupReturnsEmpty) {
  write_file(kTmpToml, minimal_valid_toml());
  PMUConfig cfg;
  ASSERT_NO_THROW(cfg.load_from_file(kTmpToml));
  EXPECT_EQ(cfg.get_pmu_event(99, 0).name, "");
}

TEST(PMUConfig, GetPmuEventOutOfBoundsEventReturnsEmpty) {
  write_file(kTmpToml, minimal_valid_toml());
  PMUConfig cfg;
  ASSERT_NO_THROW(cfg.load_from_file(kTmpToml));
  // 1 fixed + 1 schedulable = 2 valid indices (0 and 1); idx=2 is out of bounds
  EXPECT_EQ(cfg.get_pmu_event(0, 2).name, "");
}

TEST(PMUConfig, GetEventGroupByIdxOutOfBoundsReturnsEmpty) {
  write_file(kTmpToml, minimal_valid_toml());
  PMUConfig cfg;
  ASSERT_NO_THROW(cfg.load_from_file(kTmpToml));
  EXPECT_TRUE(cfg.get_event_group_by_idx(99).empty());
}

// ------------------------------------------------------------------ adaptive_grouping

TEST(PMUConfig, AdaptiveGroupingMergesGroups) {
  // Two groups sharing 1 event; union size = 3 unique events, k = 3 → merge
  write_file(kTmpToml, R"toml(
[[fixed_events]]
name        = "cpu_cycles"
description = "Cycle"
encoding    = 0x11

[[event_groups]]
events = [
  { name = "inst_spec", description = "Op spec",   encoding = 0x1b },
  { name = "ld_spec",   description = "Load spec", encoding = 0x70 },
]

[[event_groups]]
events = [
  { name = "inst_spec", description = "Op spec",    encoding = 0x1b },
  { name = "st_spec",   description = "Store spec", encoding = 0x71 },
]
)toml");
  PMUConfig cfg;
  ASSERT_NO_THROW(cfg.load_from_file(kTmpToml));
  ASSERT_EQ(cfg.get_event_group_num(), 2u);

  cfg.adaptive_grouping(3);

  EXPECT_EQ(cfg.get_event_group_num(), 1u);
  EXPECT_EQ(cfg.get_event_group_by_idx(0).size(), 3u);  // inst_spec deduplicated
}

TEST(PMUConfig, AdaptiveGroupingDoesNotMergeWhenOverBudget) {
  // Two disjoint groups of 2 events each; union = 4 > k = 3 → no merge
  write_file(kTmpToml, R"toml(
[[fixed_events]]
name        = "cpu_cycles"
description = "Cycle"
encoding    = 0x11

[[event_groups]]
events = [
  { name = "e1", description = "E1", encoding = 0x01 },
  { name = "e2", description = "E2", encoding = 0x02 },
]

[[event_groups]]
events = [
  { name = "e3", description = "E3", encoding = 0x03 },
  { name = "e4", description = "E4", encoding = 0x04 },
]
)toml");
  PMUConfig cfg;
  ASSERT_NO_THROW(cfg.load_from_file(kTmpToml));

  cfg.adaptive_grouping(3);

  EXPECT_EQ(cfg.get_event_group_num(), 2u);
}

TEST(PMUConfig, AdaptiveGroupingSingleGroupNoChange) {
  // Only 1 group: nothing to merge, should be a no-op
  write_file(kTmpToml, minimal_valid_toml());
  PMUConfig cfg;
  ASSERT_NO_THROW(cfg.load_from_file(kTmpToml));

  cfg.adaptive_grouping(8);

  EXPECT_EQ(cfg.get_event_group_num(), 1u);
}
