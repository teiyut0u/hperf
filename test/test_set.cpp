#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <vector>

// These helpers duplicate the logic under test in EventScheduler so that
// the grouping algorithm can be exercised without a real perf_event_open fd.

struct PMUEvent {
  std::string name;
  std::string desc;
  uint64_t encoding;
};

static bool less_by_encoding(const PMUEvent& a, const PMUEvent& b) {
  return a.encoding < b.encoding;
}

static std::vector<PMUEvent> union_by_encoding(std::vector<PMUEvent> a,
                                               std::vector<PMUEvent> b) {
  std::sort(a.begin(), a.end(), less_by_encoding);
  std::sort(b.begin(), b.end(), less_by_encoding);
  std::vector<PMUEvent> out;
  out.reserve(a.size() + b.size());
  std::set_union(a.begin(), a.end(), b.begin(), b.end(),
                 std::back_inserter(out), less_by_encoding);
  return out;
}

static size_t count_union_by_encoding(const std::vector<PMUEvent>& a,
                                      const std::vector<PMUEvent>& b) {
  return union_by_encoding(a, b).size();
}

static size_t get_smallest_event_group_idx(
    const std::vector<std::vector<PMUEvent>>& gs) {
  size_t idx = 0;
  for (size_t i = 1; i < gs.size(); ++i) {
    if (gs[i].size() < gs[idx].size()) idx = i;
  }
  return idx;
}

static void adaptive_grouping(std::vector<std::vector<PMUEvent>>& gs,
                              size_t k) {
  while (gs.size() >= 2) {
    size_t i = get_smallest_event_group_idx(gs);
    size_t j = 0;
    size_t best = std::numeric_limits<size_t>::max();
    for (size_t m = 0; m < gs.size(); ++m) {
      if (m == i) continue;
      size_t s = count_union_by_encoding(gs[i], gs[m]);
      if (s < best) {
        best = s;
        j = m;
      }
    }
    if (count_union_by_encoding(gs[i], gs[j]) > k) break;
    std::vector<PMUEvent> merged = union_by_encoding(gs[i], gs[j]);
    if (i > j) std::swap(i, j);
    gs.erase(gs.begin() + static_cast<ptrdiff_t>(j));
    gs.erase(gs.begin() + static_cast<ptrdiff_t>(i));
    gs.push_back(std::move(merged));
  }
}

// -------------------------------------------------------------- union_by_encoding

TEST(UnionByEncoding, DeduplicatesSharedEvents) {
  std::vector<PMUEvent> a = {{"inst_spec", "", 0x1b}, {"ld_spec", "", 0x70}};
  std::vector<PMUEvent> b = {{"inst_spec", "", 0x1b}, {"st_spec", "", 0x71}};
  auto result = union_by_encoding(a, b);
  // 0x1b appears in both → deduplicated; result has 3 distinct encodings
  EXPECT_EQ(result.size(), 3u);
}

TEST(UnionByEncoding, DisjointSetsAreConcatenated) {
  std::vector<PMUEvent> a = {{"e1", "", 0x01}, {"e2", "", 0x02}};
  std::vector<PMUEvent> b = {{"e3", "", 0x03}, {"e4", "", 0x04}};
  EXPECT_EQ(union_by_encoding(a, b).size(), 4u);
}

// -------------------------------------------------------------- adaptive_grouping

// Groups {a=4, b=4, c=3} with k=12: all three can merge because
// union(a,b,c) = 9 unique encodings ≤ 12.
TEST(AdaptiveGrouping, MergesWhenUnionFitsInK) {
  std::vector<PMUEvent> a = {{"inst_spec", "", 0x1b},
                             {"ld_spec", "", 0x70},
                             {"st_spec", "", 0x71},
                             {"dp_spec", "", 0x73}};
  std::vector<PMUEvent> b = {{"inst_spec", "", 0x1b},
                             {"vfp_spec", "", 0x75},
                             {"ase_spec", "", 0x74},
                             {"br_immed_spec", "", 0x78}};
  std::vector<PMUEvent> c = {{"inst_spec", "", 0x1b},
                             {"br_indirect_spec", "", 0x7a},
                             {"br_return_spec", "", 0x79}};

  std::vector<std::vector<PMUEvent>> groups = {a, b, c};
  adaptive_grouping(groups, 12);

  ASSERT_EQ(groups.size(), 1u) << "All 3 groups should merge into 1 with k=12";
  EXPECT_EQ(groups[0].size(), 9u) << "Merged group should contain 9 unique events";
}

// Groups are already disjoint and each pair exceeds k=3: no merging should occur.
TEST(AdaptiveGrouping, DoesNotMergeWhenUnionExceedsK) {
  std::vector<PMUEvent> a = {{"e1", "", 0x01}, {"e2", "", 0x02}};
  std::vector<PMUEvent> b = {{"e3", "", 0x03}, {"e4", "", 0x04}};

  std::vector<std::vector<PMUEvent>> groups = {a, b};
  adaptive_grouping(groups, 3);  // union = 4 > 3

  EXPECT_EQ(groups.size(), 2u) << "Groups should remain unchanged when union exceeds k";
}

// Cortex-X4 layout (groups of 9+7+8 disjoint events) with k=15:
// groups[1](7) and groups[2](8) merge (7+8=15 ≤ 15), but the resulting 15-event
// group cannot merge with groups[0](9) because 15+9=24 > 15.
TEST(AdaptiveGrouping, CortexX4LayoutWithK15) {
  std::vector<std::vector<PMUEvent>> groups = {
      {{"inst_spec", "", 0x1b},
       {"ld_spec", "", 0x70},
       {"st_spec", "", 0x71},
       {"dp_spec", "", 0x73},
       {"vfp_spec", "", 0x75},
       {"ase_spec", "", 0x74},
       {"br_immed_spec", "", 0x78},
       {"br_indirect_spec", "", 0x7a},
       {"br_return_spec", "", 0x79}},
      {{"l1d_cache_refill", "", 0x03},
       {"l1i_cache_refill", "", 0x01},
       {"l2d_cache_refill", "", 0x17},
       {"l3d_cache_refill", "", 0x2a},
       {"l1d_tlb_refill", "", 0x05},
       {"l1i_tlb_refill", "", 0x02},
       {"br_mis_pred_retired", "", 0x22}},
      {{"bus_access_rd", "", 0x60},
       {"bus_access_wr", "", 0x61},
       {"mem_access_rd", "", 0x66},
       {"mem_access_rd_percyc", "", 0x8121},
       {"dtlb_walk", "", 0x34},
       {"itlb_walk", "", 0x35},
       {"dtlb_walk_percyc", "", 0x8128},
       {"itlb_walk_percyc", "", 0x8129}}};

  adaptive_grouping(groups, 15);

  ASSERT_EQ(groups.size(), 2u) << "groups[1] and groups[2] should merge; groups[0] stays separate";
  // One group has 9 events, the other has 15
  size_t sizes[2] = {groups[0].size(), groups[1].size()};
  std::sort(std::begin(sizes), std::end(sizes));
  EXPECT_EQ(sizes[0], 9u);
  EXPECT_EQ(sizes[1], 15u);
}