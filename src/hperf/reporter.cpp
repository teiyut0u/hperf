#include "hperf/reporter.h"

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <iostream>
#include <unordered_map>

#include "hperf/expr_evaluator.h"

static inline uint64_t read_cntfrq_el0();

Reporter::Reporter(const PMUConfig& pmu_config, bool kernel_mode)
    : pmu_config_(pmu_config),
      total_time_in_ns_(0),
      prev_timestamp_(0),
      kernel_mode_(kernel_mode) {
  fixed_event_num_ = pmu_config_.get_fixed_events().size();

  int event_group_num = pmu_config_.get_event_group_num();

  if (kernel_mode_) {
    // Kernel mode: stat_[0] = fixed events, stat_[1..N] = schedulable groups
    size_t total_groups = 1 + event_group_num;
    stat_.resize(total_groups);
    stat_[0].resize(fixed_event_num_, EventStats());
    for (int i = 0; i < event_group_num; ++i) {
      stat_[1 + i].resize(pmu_config_.get_event_group_by_idx(i).size(), EventStats());
    }
    kernel_time_enabled_.resize(total_groups, 0);
    kernel_time_running_.resize(total_groups, 0);
  } else {
    // User mode: stat_[i] = fixed_events + schedulable_events for group i
    enabled_time_in_ns_.resize(event_group_num);
    stat_.resize(event_group_num);
    for (int i = 0; i < event_group_num; ++i) {
      const auto& current_event_group = pmu_config_.get_event_group_by_idx(i);
      int in_group_schedulable_event_num = current_event_group.size();
      stat_[i].resize(fixed_event_num_ + in_group_schedulable_event_num, EventStats());
    }
  }
}

void Reporter::process_a_record(const Record& record) {
  if (record.group_id < 0 || static_cast<size_t>(record.group_id) >= stat_.size() ||
      record.event_id >= stat_[record.group_id].size()) {
    std::cerr << "Warning: record out of bounds (group=" << record.group_id
              << ", event=" << record.event_id << "), skipping.\n";
    return;
  }

  // User mode: track time from timestamps
  if (!kernel_mode_) {
    if (record.timestamp > prev_timestamp_) {
      enabled_time_in_ns_[record.group_id] += (record.timestamp - prev_timestamp_);
      total_time_in_ns_ += (record.timestamp - prev_timestamp_);
      prev_timestamp_ = record.timestamp;
    }
  }

  stat_[record.group_id][record.event_id].total_value += record.value;
}

void Reporter::print_a_record(const Record& record, std::ostream& out) {
  std::string event_name;
  if (kernel_mode_) {
    // Kernel mode: group 0 = fixed events, groups 1..N = schedulable groups
    if (record.group_id == 0) {
      if (record.event_id < pmu_config_.get_fixed_events().size()) {
        event_name = pmu_config_.get_fixed_events()[record.event_id].name;
      }
    } else {
      size_t sched_group_idx = record.group_id - 1;
      if (sched_group_idx < pmu_config_.get_event_group_num()) {
        const auto& group = pmu_config_.get_event_group_by_idx(sched_group_idx);
        if (record.event_id < group.size()) {
          event_name = group[record.event_id].name;
        }
      }
    }
  } else {
    event_name = pmu_config_.get_pmu_event(record.group_id, record.event_id).name;
  }

  out << record.timestamp << ","
      << record.cpu_id << ","
      << record.group_id + 1 << ","
      << event_name << ","
      << record.value << "\n";
}

void Reporter::add_kernel_time(int group_id, uint64_t time_enabled, uint64_t time_running) {
  if (group_id < 0 || static_cast<size_t>(group_id) >= kernel_time_enabled_.size()) return;
  kernel_time_enabled_[group_id] += time_enabled;
  kernel_time_running_[group_id] += time_running;
}

void Reporter::set_total_time(uint64_t total_time_in_ns) {
  total_time_in_ns_ = total_time_in_ns;
}

void Reporter::estimation() {
  if (kernel_mode_) {
    estimation_kernel_mode_();
    return;
  }

  const auto event_group_num = pmu_config_.get_event_group_num();

  for (size_t j = 0; j < fixed_event_num_; j++) {
    uint64_t fixed_event_total = 0;
    for (size_t i = 0; i < event_group_num; i++) {
      fixed_event_total += stat_[i][j].total_value;
    }
    stat_[0][j].estimated_value = fixed_event_total;
  }

  for (size_t i = 0; i < event_group_num; i++) {
    if (enabled_time_in_ns_[i] == 0) continue;
    double ratio = static_cast<double>(total_time_in_ns_) / static_cast<double>(enabled_time_in_ns_[i]);
    for (size_t j = 0; j < pmu_config_.get_event_group_by_idx(i).size(); ++j) {
      stat_[i][fixed_event_num_ + j].estimated_value = static_cast<uint64_t>(stat_[i][fixed_event_num_ + j].total_value * ratio);
    }
  }
}

void Reporter::estimation_kernel_mode_() {
  // Group 0: pinned fixed events — no scaling needed
  for (size_t j = 0; j < fixed_event_num_; j++) {
    stat_[0][j].estimated_value = stat_[0][j].total_value;
  }

  // Groups 1..N: schedulable events — scale by time_enabled / time_running
  const auto event_group_num = pmu_config_.get_event_group_num();
  for (size_t i = 0; i < event_group_num; i++) {
    size_t stat_idx = 1 + i;
    uint64_t time_running = kernel_time_running_[stat_idx];
    uint64_t time_enabled = kernel_time_enabled_[stat_idx];
    double ratio = (time_running > 0) ? static_cast<double>(time_enabled) / static_cast<double>(time_running) : 1.0;
    for (size_t j = 0; j < stat_[stat_idx].size(); j++) {
      stat_[stat_idx][j].estimated_value = static_cast<uint64_t>(stat_[stat_idx][j].total_value * ratio);
    }
  }
}

std::string Reporter::format_with_commas_(uint64_t value) {
  std::string str = std::to_string(value);
  auto len = str.length();
  for (size_t i = 3; i < len; i += 3) {
    str.insert(len - i, ",");
  }
  return str;
}

void Reporter::print_stats() {
  std::cout << "========== Performance Statistics ==========\n";
  std::cout << std::fixed << std::setprecision(2);

  // Fixed events — same layout (stat_[0]) in both modes
  if (kernel_mode_) {
    std::cout << "Fixed events (pinned, " << total_time_in_ns_ / 1e6 << " ms, 100.00 %)\n";
  } else {
    std::cout << "Fixed events (" << total_time_in_ns_ / 1e6 << " ms, 100.00 %)\n";
  }
  for (size_t event_id = 0; event_id < fixed_event_num_; ++event_id) {
    print_event_count_(stat_[0][event_id].estimated_value, pmu_config_.get_fixed_events()[event_id].name);
  }

  // Schedulable event groups — group header differs by mode
  for (size_t group_id = 0; group_id < pmu_config_.get_event_group_num(); ++group_id) {
    if (kernel_mode_) {
      size_t stat_idx = 1 + group_id;
      uint64_t time_enabled = kernel_time_enabled_[stat_idx];
      uint64_t time_running = kernel_time_running_[stat_idx];
      double running_pct = (time_enabled > 0)
                               ? static_cast<double>(time_running) * 100.0 / static_cast<double>(time_enabled)
                               : 0.0;
      std::cout << "Group " << (group_id + 1) << " (" << time_running / 1e6 << " ms running, "
                << running_pct << " %)\n";

      const auto& current_group = pmu_config_.get_event_group_by_idx(group_id);
      for (size_t event_id = 0; event_id < stat_[stat_idx].size(); ++event_id) {
        print_event_count_(stat_[stat_idx][event_id].estimated_value, current_group[event_id].name);
      }
    } else {
      double percentage = total_time_in_ns_ > 0
                              ? static_cast<double>(enabled_time_in_ns_[group_id]) * 100.0 / static_cast<double>(total_time_in_ns_)
                              : 0.0;
      std::cout << "Group " << (group_id + 1) << " (" << enabled_time_in_ns_[group_id] / 1e6 << " ms, "
                << percentage << " %)\n";

      const auto& current_group = pmu_config_.get_event_group_by_idx(group_id);
      for (size_t event_id = fixed_event_num_; event_id < stat_[group_id].size(); ++event_id) {
        print_event_count_(stat_[group_id][event_id].estimated_value, current_group[event_id - fixed_event_num_].name);
      }
    }
  }
}

void Reporter::print_metrics() {
  std::cout << "=========== Performance Metrics ============\n";

  // Build variable map: event name -> estimated_value as double
  // The by-name helpers handle both kernel and user mode indexing.
  std::unordered_map<std::string, double> vars;
  for (const auto& ev : pmu_config_.get_fixed_events())
    vars[ev.name] = static_cast<double>(get_fixed_event_stat_by_name(ev.name).estimated_value);
  for (size_t g = 0; g < pmu_config_.get_event_group_num(); ++g)
    for (const auto& ev : pmu_config_.get_event_group_by_idx(g))
      vars[ev.name] = static_cast<double>(get_schedulable_event_stat_by_name(ev.name).estimated_value);

  vars["total_time_ns"] = static_cast<double>(total_time_in_ns_);
  vars["cntfrq"] = static_cast<double>(read_cntfrq_el0());

  for (const auto& section : pmu_config_.get_metric_sections()) {
    std::cout << section.title << ":\n";
    for (const auto& item : section.items) {
      double value = ExprEvaluator::evaluate(item.expr, vars);
      print_metric_(value, item.type, item.name);
    }
  }

  std::cout << "============================================\n";
}

EventStats Reporter::get_schedulable_event_stat_by_name(const std::string& name) {
  for (size_t group_id = 0; group_id < pmu_config_.get_event_group_num(); ++group_id) {
    const auto& schedulable_events = pmu_config_.get_event_group_by_idx(group_id);
    for (size_t event_id = 0; event_id < schedulable_events.size(); ++event_id) {
      if (schedulable_events[event_id].name == name) {
        size_t stat_group = kernel_mode_ ? (1 + group_id) : group_id;
        size_t stat_event = kernel_mode_ ? event_id : (fixed_event_num_ + event_id);
        return stat_[stat_group][stat_event];
      }
    }
  }
  return EventStats();
}

EventStats Reporter::get_fixed_event_stat_by_name(const std::string& name) {
  const auto& fixed_events = pmu_config_.get_fixed_events();
  for (size_t event_id = 0; event_id < fixed_events.size(); ++event_id) {
    if (fixed_events[event_id].name == name) {
      return stat_[0][event_id];  // both modes: fixed events are in stat_[0]
    }
  }
  return EventStats();
}

/**
 * @brief Read the system counter frequency.
 *
 * On AArch64: reads CNTFRQ_EL0 via inline assembly (typically 19.2 MHz on
 * Android/Linux ARM boards).
 * On other architectures: returns 0 as a placeholder — TODO: implement via
 * clock_gettime or /proc/cpuinfo parsing for x86 Linux servers.
 */
static inline uint64_t read_cntfrq_el0() {
#if defined(__aarch64__)
  uint64_t freq;
  __asm__ volatile(
      "mrs %0, CNTFRQ_EL0\n"
      : "=r"(freq)
      :
      : "memory");
  return freq;
#else
  return 0;
#endif
}

void Reporter::print_event_count_(uint64_t c, const std::string& event_name) {
  std::cout << "  " << std::left << std::setw(22) << event_name
            << std::right << std::setw(20) << format_with_commas_(c) << '\n';
}

void Reporter::print_metric_(double value, const std::string& type, const std::string& name) {
  std::cout << std::fixed;
  if (type == "percentage") {
    print_percentage_(value, name);
  } else if (type == "GHz") {
    print_GHz_(value, name);
  } else if (type == "cycles") {
    print_cycles_(value, name);
  } else {
    // "decimal" and any unknown type
    print_decimal_(value, name);
  }
}

void Reporter::print_percentage_(double value, const std::string& metric_name) {
  std::cout << "  " << std::left << std::setw(27) << metric_name
            << std::right << std::setw(13) << std::fixed << std::setprecision(2)
            << value * 100.0 << " %\n";
}

void Reporter::print_decimal_(double value, const std::string& metric_name) {
  std::cout << "  " << std::left << std::setw(30) << metric_name
            << std::right << std::setw(12) << std::fixed << std::setprecision(4) << value << '\n';
}

void Reporter::print_cycles_(double value, const std::string& metric_name) {
  std::cout << "  " << std::left << std::setw(23) << metric_name
            << std::right << std::setw(12) << std::fixed << std::setprecision(4)
            << value << " cycles\n";
}

void Reporter::print_GHz_(double value, const std::string& metric_name) {
  std::cout << "  " << std::left << std::setw(22) << metric_name
            << std::right << std::setw(16) << std::fixed << std::setprecision(4)
            << value << " GHz\n";
}
