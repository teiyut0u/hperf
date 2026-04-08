#include "hperf/pmu_config.h"

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <limits>
#include <ostream>
#include <vector>

#include "hperf/hperf_error.h"
#include "hperf/pmu_event.h"

#define TOML_EXCEPTIONS 0  // Use error_code instead of exceptions for parse errors
#include <toml.hpp>

namespace {

// Returns the index of the group with the fewest events.
size_t smallest_group_idx(const std::vector<std::vector<PMUEvent>>& groups) {
  size_t idx = 0;
  for (size_t i = 1; i < groups.size(); ++i) {
    if (groups[i].size() < groups[idx].size()) idx = i;
  }
  return idx;
}

bool pmu_event_less(const PMUEvent& a, const PMUEvent& b) {
  return a.encoding < b.encoding;
}

// Returns the number of distinct events in the union of a and b (dedup by encoding).
size_t event_union_size(const std::vector<PMUEvent>& a, const std::vector<PMUEvent>& b) {
  auto a_sorted = a;
  auto b_sorted = b;
  std::sort(a_sorted.begin(), a_sorted.end(), pmu_event_less);
  std::sort(b_sorted.begin(), b_sorted.end(), pmu_event_less);
  std::vector<PMUEvent> out;
  out.reserve(a_sorted.size() + b_sorted.size());
  std::set_union(a_sorted.begin(), a_sorted.end(), b_sorted.begin(), b_sorted.end(),
                 std::back_inserter(out), pmu_event_less);
  return out.size();
}

// Returns the union of a and b (dedup by encoding).
std::vector<PMUEvent> event_union(std::vector<PMUEvent> a, std::vector<PMUEvent> b) {
  std::sort(a.begin(), a.end(), pmu_event_less);
  std::sort(b.begin(), b.end(), pmu_event_less);
  std::vector<PMUEvent> out;
  out.reserve(a.size() + b.size());
  std::set_union(a.begin(), a.end(), b.begin(), b.end(),
                 std::back_inserter(out), pmu_event_less);
  return out;
}

}  // namespace

PMUConfig::PMUConfig() {}

void PMUConfig::load_from_file(const std::string& path) {
  auto result = toml::parse_file(path);
  if (!result) {
    throw HperfError("Failed to parse config file '" + path + "': " +
                     std::string(result.error().description()));
  }
  const auto& tbl = result.table();

  // ── Parse fixed_events ──────────────────────────────────────────────────
  auto fixed_arr = tbl["fixed_events"].as_array();
  if (!fixed_arr) {
    throw HperfError("'fixed_events' array missing in config file '" + path + "'.");
  }
  fixed_events_.clear();
  for (const auto& elem : *fixed_arr) {
    const auto* ev = elem.as_table();
    if (!ev) continue;
    PMUEvent pmu_ev;
    auto name_node = (*ev)["name"].value<std::string>();
    auto desc_node = (*ev)["description"].value<std::string>();
    auto enc_node = (*ev)["encoding"].value<int64_t>();
    if (!name_node || !desc_node || !enc_node) {
      throw HperfError("Each fixed_event entry must have 'name', 'description', 'encoding'.");
    }
    pmu_ev.name = *name_node;
    pmu_ev.description = *desc_node;
    pmu_ev.encoding = static_cast<uint64_t>(*enc_node);
    fixed_events_.push_back(std::move(pmu_ev));
  }

  // ── Parse event_groups ───────────────────────────────────────────────────
  auto groups_arr = tbl["event_groups"].as_array();
  if (!groups_arr) {
    throw HperfError("'event_groups' array missing in config file '" + path + "'.");
  }
  event_groups_.clear();
  for (const auto& group_elem : *groups_arr) {
    const auto* group_tbl = group_elem.as_table();
    if (!group_tbl) continue;
    auto events_arr = (*group_tbl)["events"].as_array();
    if (!events_arr) {
      throw HperfError("Each event_group entry must have an 'events' array.");
    }
    std::vector<PMUEvent> group;
    for (const auto& ev_elem : *events_arr) {
      const auto* ev = ev_elem.as_table();
      if (!ev) continue;
      PMUEvent pmu_ev;
      auto name_node = (*ev)["name"].value<std::string>();
      auto desc_node = (*ev)["description"].value<std::string>();
      auto enc_node = (*ev)["encoding"].value<int64_t>();
      if (!name_node || !desc_node || !enc_node) {
        throw HperfError("Each event entry must have 'name', 'description', 'encoding'.");
      }
      pmu_ev.name = *name_node;
      pmu_ev.description = *desc_node;
      pmu_ev.encoding = static_cast<uint64_t>(*enc_node);
      group.push_back(std::move(pmu_ev));
    }
    event_groups_.push_back(std::move(group));
  }

  // ── Parse metric_sections (optional) ────────────────────────────────────
  metric_sections_.clear();
  auto metrics_arr = tbl["metric_sections"].as_array();
  if (metrics_arr) {
    for (const auto& sec_elem : *metrics_arr) {
      const auto* sec_tbl = sec_elem.as_table();
      if (!sec_tbl) continue;
      MetricSection section;
      auto title_node = (*sec_tbl)["title"].value<std::string>();
      if (!title_node) {
        throw HperfError("Each metric_section must have a 'title' field.");
      }
      section.title = *title_node;

      auto items_arr = (*sec_tbl)["items"].as_array();
      if (items_arr) {
        for (const auto& item_elem : *items_arr) {
          const auto* item_tbl = item_elem.as_table();
          if (!item_tbl) continue;
          MetricItem item;
          auto iname = (*item_tbl)["name"].value<std::string>();
          auto itype = (*item_tbl)["type"].value<std::string>();
          auto iexpr = (*item_tbl)["expr"].value<std::string>();
          if (!iname || !itype || !iexpr) {
            throw HperfError("Each metric item must have 'name', 'type', 'expr'.");
          }
          item.name = *iname;
          item.type = *itype;
          item.expr = *iexpr;
          section.items.push_back(std::move(item));
        }
      }
      metric_sections_.push_back(std::move(section));
    }
  }

  // ── Final validation ─────────────────────────────────────────────────────
  if (!is_valid()) {
    throw HperfError("PMU config '" + path + "' is invalid: fixed_events and all event_groups must be non-empty.");
  }
}

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
    std::cerr << "Error: Invalid event group index." << '\n';
    static const std::vector<PMUEvent> empty{};
    return empty;
  } else {
    return event_groups_[idx];
  }
}

size_t PMUConfig::get_event_group_num() const {
  return event_groups_.size();
}

const std::vector<MetricSection>& PMUConfig::get_metric_sections() const {
  return metric_sections_;
}

void PMUConfig::print_pmu_config() const {
  std::cout << "Fixed events" << '\n';
  for (const auto& pmu_event : fixed_events_) {
    std::cout << "  " << pmu_event.name << ": " << pmu_event.description
              << " (0x" << std::hex << pmu_event.encoding << std::dec << ")" << '\n';
  }

  for (size_t idx = 0; idx < event_groups_.size(); ++idx) {
    std::cout << "Event group #" << idx + 1 << '\n';
    for (const auto& pmu_event : event_groups_[idx]) {
      std::cout << "  " << pmu_event.name << ": " << pmu_event.description
                << " (0x" << std::hex << pmu_event.encoding << std::dec << ")" << '\n';
    }
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
    std::cout << " }" << '\n';
  }
}

void PMUConfig::adaptive_grouping(size_t programmable_counters_num) {
  while (event_groups_.size() >= 2) {
    // Find i: the group with the fewest events
    size_t i = smallest_group_idx(event_groups_);

    // Find j: the group that, when merged with i, yields the smallest union
    size_t j = 0;
    size_t best_size = std::numeric_limits<size_t>::max();
    for (size_t k = 0; k < event_groups_.size(); ++k) {
      if (k != i) {
        size_t s = event_union_size(event_groups_[i], event_groups_[k]);
        if (s < best_size) {
          best_size = s;
          j = k;
        }
      }
    }

    if (event_union_size(event_groups_[i], event_groups_[j]) <= programmable_counters_num) {
      std::vector<PMUEvent> merged = event_union(event_groups_[i], event_groups_[j]);
      if (i > j) std::swap(i, j);
      event_groups_.erase(event_groups_.begin() + j);
      event_groups_.erase(event_groups_.begin() + i);
      event_groups_.push_back(merged);
    } else {
      break;
    }
  }
}
