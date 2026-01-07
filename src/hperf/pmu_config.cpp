#include "hperf/pmu_config.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <ostream>
#include <vector>
#include <limits>

#include "hperf/pmu_event.h"

// Include the specified PMU config header based on compilation option.
// In this header, a global `pmu_config` is defined.
#if defined(CPU_N1)
#include "hperf/pmu_config/cpu_n1.h"
#elif defined(CPU_TAISHAN)
#include "hperf/pmu_config/cpu_taishan.h"
#elif defined(CPU_ICX)
#include "hperf/pmu_config/cpu_icx.h"
#elif defined(CPU_CLX)
#include "hperf/pmu_config/cpu_clx.h"
#else
#error "No CPU model defined."
#endif

PMUConfig::PMUConfig() : fixed_events_(::fixed_events), event_groups_(::event_groups) {}

bool PMUConfig::is_valid() const {
  if (fixed_events_.empty() || event_groups_.empty()) {
    return false;
  } else {
    for (const auto& event_group : event_groups_) {
      if (event_group.empty())
        return false;
    }
    return true;
  }
}

const PMUEvent& PMUConfig::get_pmu_event(size_t group_idx, size_t event_idx) const {
  static const PMUEvent empty{};
  if (group_idx >= event_groups_.size()) {
    return empty;
  }
  if (event_idx >= (fixed_events_.size() + event_groups_[group_idx].size())) {
    return empty;
  } else {
    return event_idx < fixed_events_.size() ? fixed_events_[event_idx] : event_groups_[group_idx][event_idx - fixed_events_.size()];
  }
}

const std::vector<PMUEvent>& PMUConfig::get_fixed_events() const {
  return fixed_events_;
}

const std::vector<PMUEvent>& PMUConfig::get_event_group_by_idx(size_t idx) const {
  if (idx >= event_groups_.size()) {
    std::cerr << "Error: Invalid event group index." << std::endl;
    static const std::vector<PMUEvent> empty{};
    return empty;
  } else {
    return event_groups_[idx];
  }
}

size_t PMUConfig::get_event_group_num() const {
  return event_groups_.size();
}

void PMUConfig::print_pmu_config() const {
  std::cout << "Fixed events" << std::endl;
  for (const auto& pmu_event : fixed_events_) {
    std::cout << "  " << pmu_event.name << ": " << pmu_event.description
              << " (0x" << std::hex << pmu_event.encoding << std::dec << ")" << std::endl;
  }

  int idx = 0;
  for (const auto& event_group : event_groups_) {
    std::cout << "Event group #" << idx + 1 << std::endl;
    for (const auto& pmu_event : event_group) {
      std::cout << "  " << pmu_event.name << ": " << pmu_event.description
                << " (0x" << std::hex << pmu_event.encoding << std::dec << ")" << std::endl;
    }
    ++idx;
  }
}

void PMUConfig::print_event_groups_by_line() const {
  for (size_t i = 0; i < event_groups_.size(); ++i) {
    std::cout << "[" << i << "]: ";

    std::cout << "{ ";
    for (size_t j = 0; j < event_groups_[i].size(); ++j) {
      std::cout << event_groups_[i][j].name;
      if (j < event_groups_[i].size() - 1) std::cout << ", ";
    }
    std::cout << " }" << std::endl;
  }
}

void PMUConfig::adaptive_grouping(size_t programmable_counters_num) {
  auto get_smallest_event_group_idx = [](const std::vector<std::vector<PMUEvent>>& event_groups) {
    size_t smallest_idx = 0;
    for (size_t i = 1; i < event_groups.size(); ++i) {
      if (event_groups[i].size() < event_groups[smallest_idx].size()) {
        smallest_idx = i;
      }
    }
    return smallest_idx;
  };

  auto less_by_encoding = [](const PMUEvent& a, const PMUEvent& b) {
    return a.encoding < b.encoding;
  };

  auto count_union_by_encoding = [&less_by_encoding](const std::vector<PMUEvent>& a,
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
  };

  auto union_by_encoding = [&less_by_encoding](std::vector<PMUEvent> a,
                                               std::vector<PMUEvent> b) {
    std::sort(a.begin(), a.end(), less_by_encoding);
    std::sort(b.begin(), b.end(), less_by_encoding);

    std::vector<PMUEvent> out;
    out.reserve(a.size() + b.size());
    std::set_union(a.begin(), a.end(), b.begin(), b.end(),
                   std::back_inserter(out), less_by_encoding);
    return out;
  };

  while (true) {
    if (event_groups_.size() < 2)
      break;

    // Find i, G_i = argmin(|G|)
    size_t i = get_smallest_event_group_idx(event_groups_);

    // Find j, G_j = argmin(|G_i U G|)
    size_t j = 0;
    size_t merged_size = std::numeric_limits<size_t>::max();
    for (size_t k = 0; k < event_groups_.size(); ++k) {
      if (k != i) {
        size_t current_merged_size = count_union_by_encoding(event_groups_[i], event_groups_[k]);
        if (current_merged_size < merged_size) {
          merged_size = current_merged_size;
          j = k;
        }
      }
    }

    if (count_union_by_encoding(event_groups_[i], event_groups_[j]) <= programmable_counters_num) {
      std::vector<PMUEvent> merged = union_by_encoding(event_groups_[i], event_groups_[j]);
      if (i > j) std::swap(i, j);
      event_groups_.erase(event_groups_.begin() + j);
      event_groups_.erase(event_groups_.begin() + i);
      event_groups_.push_back(merged);
    } else {
      break;
    }
  }
}