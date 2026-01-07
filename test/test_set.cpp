#include <linux/perf_event.h>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <ostream>
#include <vector>
#include <cstdint>
#include <limits>

struct PMUEvent {
  std::string name;
  std::string desc;
  uint64_t encoding;
};

static bool less_by_encoding(const PMUEvent& a, const PMUEvent& b) {
  return a.encoding < b.encoding;
}
static bool equal_by_encoding(const PMUEvent& a, const PMUEvent& b) {
  return a.encoding == b.encoding;
}

std::vector<PMUEvent> union_by_encoding(std::vector<PMUEvent> a,
                                        std::vector<PMUEvent> b) {
  std::sort(a.begin(), a.end(), less_by_encoding);
  std::sort(b.begin(), b.end(), less_by_encoding);

  std::vector<PMUEvent> out;
  out.reserve(a.size() + b.size());
  std::set_union(a.begin(), a.end(), b.begin(), b.end(),
                 std::back_inserter(out), less_by_encoding);
  return out;
}

void print_event_group(const std::vector<PMUEvent>& events) {
  std::cout << "{ ";
  for (size_t i = 0; i < events.size(); ++i) {
    std::cout << events[i].name;
    if (i < events.size() - 1) std::cout << ", ";
  }
  std::cout << " }" << std::endl;
}

void print_event_groups(const std::vector<std::vector<PMUEvent>>& event_groups) {
  for (size_t i = 0; i < event_groups.size(); ++i) {
    std::cout << "[" << i << "] ";
    print_event_group(event_groups[i]);
  }
}

size_t get_smallest_event_group_idx(const std::vector<std::vector<PMUEvent>>& event_groups) {
  size_t smallest_idx = 0;
  for (size_t i = 1; i < event_groups.size(); ++i) {
    if (event_groups[i].size() < event_groups[smallest_idx].size()) {
      smallest_idx = i;
    }
  }
  return smallest_idx;
}

size_t count_union_by_encoding(const std::vector<PMUEvent>& a,
                               const std::vector<PMUEvent>& b) {
  std::vector<PMUEvent> a_sorted = a;
  std::vector<PMUEvent> b_sorted = b;
  std::sort(a_sorted.begin(), a_sorted.end(), less_by_encoding);
  std::sort(b_sorted.begin(), b_sorted.end(), less_by_encoding);

  std::vector<PMUEvent> out;
  out.reserve(a_sorted.size() + b_sorted.size());
  std::set_union(a_sorted.begin(), a_sorted.end(), b_sorted.begin(), b_sorted.end(),
                 std::back_inserter(out), less_by_encoding);
  return out.size();
}

void adaptive_grouping(std::vector<std::vector<PMUEvent>>& event_groups, size_t k) {
  while (true) {
    if (event_groups.size() < 2)
      break;

    // Find i, G_i = argmin(|G|)
    size_t i = get_smallest_event_group_idx(event_groups);

    // Find j, G_j = argmin(|G_i U G|)
    size_t j = 0;
    size_t merged_size = std::numeric_limits<size_t>::max();
    for (size_t k = 0; k < event_groups.size(); ++k) {
      if (k != i) {
        size_t current_merged_size = count_union_by_encoding(event_groups[i], event_groups[k]);
        if (current_merged_size < merged_size) {
          merged_size = current_merged_size;
          j = k;
        }
      }
    }

    if (count_union_by_encoding(event_groups[i], event_groups[j]) <= k) {
      std::vector<PMUEvent> merged = union_by_encoding(event_groups[i], event_groups[j]);
      if (i > j) std::swap(i, j);
      event_groups.erase(event_groups.begin() + j);
      event_groups.erase(event_groups.begin() + i);
      event_groups.push_back(merged);
    } else {
      break;
    }
  }
}

int main() {
  std::vector<PMUEvent> a = {{"inst_spec", "Operation speculatively executed", 0x1b},
                             {"ld_spec", "Operation speculatively executed, load", 0x70},
                             {"st_spec", "Operation speculatively executed, store", 0x71},
                             {"dp_spec", "Operation speculatively executed, integer data processing", 0x73}};
  std::vector<PMUEvent> b = {{"inst_spec", "Operation speculatively executed", 0x1b},
                             {"vfp_spec", "Operation speculatively executed, scalar floating-point", 0x75},
                             {"ase_spec", "Operation speculatively executed, Advanced SIMD", 0x74},
                             {"br_immed_spec", "Branch Speculatively executed, immediate branch", 0x78}};
  std::vector<PMUEvent> c = {{"inst_spec", "Operation speculatively executed", 0x1b},
                             {"br_indirect_spec", "Branch Speculatively executed, indirect branch", 0x7a},
                             {"br_return_spec", "Branch Speculatively executed, procedure return", 0x79}};

  std::cout << "Before:" << std::endl;
  print_event_group(a);
  print_event_group(b);

  std::cout << "After:" << std::endl;
  std::vector<PMUEvent> merged_event_group = union_by_encoding(a, b);
  print_event_group(merged_event_group);

  std::vector<std::vector<PMUEvent>> event_groups = {a, b, c};
  size_t smallest_event_group_idx = get_smallest_event_group_idx(event_groups);

  std::cout << "Smallest event group: " << std::endl;
  print_event_group(event_groups[smallest_event_group_idx]);

  std::cout << "Adaptive Grouping: " << std::endl;
  std::cout << "Before:" << std::endl;
  print_event_groups(event_groups);

  adaptive_grouping(event_groups, 12);

  std::cout << "After:" << std::endl;
  print_event_groups(event_groups);

  std::vector<std::vector<PMUEvent>> cortex_event_groups = {
      {{"inst_spec", "Operation speculatively executed", 0x1b},
       {"ld_spec", "Operation speculatively executed, load", 0x70},
       {"st_spec", "Operation speculatively executed, store", 0x71},
       {"dp_spec", "Operation speculatively executed, integer data processing", 0x73},
       {"vfp_spec", "Operation speculatively executed, scalar floating-point", 0x75},
       {"ase_spec", "Operation speculatively executed, Advanced SIMD", 0x74},
       {"br_immed_spec", "Branch Speculatively executed, immediate branch", 0x78},
       {"br_indirect_spec", "Branch Speculatively executed, indirect branch", 0x7a},
       {"br_return_spec", "Branch Speculatively executed, procedure return", 0x79}},
      {{"l1d_cache_refill", "Level 1 data cache refill", 0x03},
       {"l1i_cache_refill", "Level 1 instruction cache refill", 0x01},
       {"l2d_cache_refill", "Level 2 data cache refill", 0x17},
       {"l3d_cache_refill", "Attributable level 3 cache refill", 0x2a},
       {"l1d_tlb_refill", "Level 1 data TLB refill", 0x05},
       {"l1i_tlb_refill", "Level 1 instruction TLB refill", 0x02},
       {"br_mis_pred_retired", "Branch Instruction architecturally executed, mispredicted", 0x22}},
      {{"bus_access_rd", "Bus access, read", 0x60},
       {"bus_access_wr", "Bus access, write", 0x61},
       {"mem_access_rd", "Data memory access, read", 0x66},
       {"mem_access_rd_percyc", "Total cycles, mem_access_rd", 0x8121},
       {"dtlb_walk", "Data TLB access with at least one translation table walk", 0x34},
       {"itlb_walk", "Instruction TLB access with at least one translation table walk", 0x35},
       {"dtlb_walk_percyc", "Total cycles, dtlb_walk", 0x8128},
       {"itlb_walk_percyc", "Total cycles, itlb_walk", 0x8129}}};

  std::cout << "Adaptive Grouping: " << std::endl;
  std::cout << "Before:" << std::endl;
  print_event_groups(cortex_event_groups);

  adaptive_grouping(cortex_event_groups, 15);

  std::cout << "After:" << std::endl;
  print_event_groups(cortex_event_groups);

  return 0;
}
