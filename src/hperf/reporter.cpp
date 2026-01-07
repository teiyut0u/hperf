/**
 * @file reporter.cpp
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2025-09-09
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "hperf/reporter.h"

#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <ios>
#include <iostream>

static inline uint64_t read_cntfrq_el0(void);

Reporter::Reporter(const PMUConfig& pmu_config)
    : pmu_config_(pmu_config),
      total_time_in_ns_(0),
      prev_timestamp_(0) {
  fixed_event_num_ = pmu_config_.get_fixed_events().size();

  int event_group_num = pmu_config_.get_event_group_num();
  enabled_time_in_ns_.resize(event_group_num);
  stat_.resize(event_group_num);

  for (int i = 0; i < event_group_num; ++i) {
    const auto& current_event_group = pmu_config_.get_event_group_by_idx(i);
    int in_group_schedulable_event_num = current_event_group.size();
    stat_[i].resize(fixed_event_num_ + in_group_schedulable_event_num, EventStats());
  }
}

Reporter::~Reporter() {}

void Reporter::process_a_record(const Record& record) {
  if (record.timestamp > prev_timestamp_) {
    enabled_time_in_ns_[record.group_id] += (record.timestamp - prev_timestamp_);
    total_time_in_ns_ += (record.timestamp - prev_timestamp_);
    prev_timestamp_ = record.timestamp;
  }

  stat_[record.group_id][record.event_id].total_value += record.value;
}

void Reporter::print_a_record(const Record& record, std::ostream& out) {
  out << record.timestamp << ","
      << record.cpu_id << ","
      << record.group_id + 1 << ","
      << pmu_config_.get_pmu_event(record.group_id, record.event_id).name << ","
      << record.value << "\n";
}

void Reporter::estimation() {
  const auto event_group_num = pmu_config_.get_event_group_num();

  for (int j = 0; j < fixed_event_num_; j++) {
    uint64_t fixed_event_total = 0;
    for (int i = 0; i < event_group_num; i++) {
      fixed_event_total += stat_[i][j].total_value;
    }
    stat_[0][j].estimated_value = fixed_event_total;
  }

  for (int i = 0; i < event_group_num; i++) {
    for (int j = 0; j < pmu_config_.get_event_group_by_idx(i).size(); j++) {
      double ratio = (double)total_time_in_ns_ / enabled_time_in_ns_[i];
      stat_[i][fixed_event_num_ + j].estimated_value = (uint64_t)(stat_[i][fixed_event_num_ + j].total_value * ratio);
    }
  }
}

std::string Reporter::format_with_commas_(uint64_t value) {
  std::string str = std::to_string(value);
  int n = str.length() - 3;
  while (n > 0) {
    str.insert(n, ",");
    n -= 3;
  }
  return str;
}

void Reporter::print_stats() {
  std::cout << "========== Performance Statistics ==========\n";
  std::cout << std::fixed << std::setprecision(2);

  // fixed events
  std::cout << "Fixed events (" << total_time_in_ns_ / 1e6 << " ms, 100.00 %)\n";
  for (size_t event_id = 0; event_id < fixed_event_num_; ++event_id) {
    const auto& event_stat = stat_[0][event_id];
    const auto& pmu_event = pmu_config_.get_fixed_events()[event_id];
    print_event_count_(event_stat.estimated_value, pmu_event.name);
  }

  // other events
  for (size_t group_id = 0; group_id < pmu_config_.get_event_group_num(); ++group_id) {
    double percentage = (double)enabled_time_in_ns_[group_id] * 100.0 / total_time_in_ns_;
    std::cout << "Group " << (group_id + 1) << " (" << enabled_time_in_ns_[group_id] / 1e6 << " ms, "
              << percentage << " %)\n";

    const auto& current_group = pmu_config_.get_event_group_by_idx(group_id);

    for (size_t event_id = fixed_event_num_; event_id < stat_[group_id].size(); ++event_id) {
      const auto& event_stat = stat_[group_id][event_id];
      const auto& pmu_event = current_group[event_id - fixed_event_num_];
      print_event_count_(event_stat.estimated_value, pmu_event.name);
    }
  }
}

void Reporter::print_metrics() {
  std::cout << "=========== Performance Metrics ============\n";

#if defined(CPU_ORYON)
  print_metrics_oryon_();
#elif defined(CPU_CORTEX_X4)
  print_metrics_cortex_x4_();
#endif

  std::cout << "============================================\n";
}

EventStats Reporter::get_event_stat_by_name(std::string name, size_t group_id) {
  if (group_id >= pmu_config_.get_event_group_num()) {
    return EventStats();
  }

  const auto& fixed_events = pmu_config_.get_fixed_events();
  const auto& schedulable_events = pmu_config_.get_event_group_by_idx(group_id);
  
  for (size_t event_id = 0; event_id < fixed_events.size(); ++event_id) {
    if (fixed_events[event_id].name == name) {
      return stat_[group_id][event_id];
    }
  }
  for (size_t event_id = 0; event_id < schedulable_events.size(); ++event_id) {
    if (schedulable_events[event_id].name == name) {
      return stat_[group_id][fixed_event_num_ + event_id];
    }
  }

  // if not found, return default EventStats
  return EventStats();
}

EventStats Reporter::get_schedulable_event_stat_by_name(std::string name, size_t& group_id) {
  for (size_t group_id = 0; group_id < pmu_config_.get_event_group_num(); ++group_id) {
    const auto& schedulable_events = pmu_config_.get_event_group_by_idx(group_id);
    for (size_t event_id = 0; event_id < schedulable_events.size(); ++event_id) {
      if (schedulable_events[event_id].name == name) {
        return stat_[group_id][fixed_event_num_ + event_id];
      }
    }
  }

  // if not found, return default EventStats
  return EventStats();
}

EventStats Reporter::get_fixed_event_stat_by_name(std::string name, size_t group_id) {
  if (group_id >= pmu_config_.get_event_group_num()) {
    return EventStats();
  }

  const auto& fixed_events = pmu_config_.get_fixed_events();

  for (size_t event_id = 0; event_id < fixed_events.size(); ++event_id) {
    if (fixed_events[event_id].name == name) {
      return stat_[group_id][event_id];
    }
  }

  // if not found, return default EventStats
  return EventStats();
}

/**
 * @brief Read CNTFRQ_EL0 register
 * @return The frequency value in Hz
 *
 * CNTFRQ_EL0 is a 64-bit register that holds the frequency of the system
 * counter. We use inline assembly to read this register.
 */
static inline uint64_t read_cntfrq_el0(void) {
  uint64_t freq;

  /*
   * ARM assembly to read CNTFRQ_EL0 register
   * mrs = Move System Register
   * %0 = output operand (freq variable)
   * "r" = register constraint
   */
  __asm__ volatile(
      "mrs %0, CNTFRQ_EL0\n"
      : "=r"(freq)
      :
      : "memory");

  return freq;
}

void Reporter::print_metrics_oryon_() {
  std::cout << "Pipeline basic metrics:\n";
  uint64_t cpu_cycles = get_fixed_event_stat_by_name("cpu_cycles", 0).estimated_value;
  uint64_t inst_retired = get_fixed_event_stat_by_name("inst_retired", 0).estimated_value;
  uint64_t cnt_cycles = get_fixed_event_stat_by_name("cnt_cycles", 0).estimated_value;
  uint64_t cnt_freq = read_cntfrq_el0();

  print_decimal_(cpu_cycles, inst_retired, "CPI");
  print_percentage_(cnt_cycles * 1e9, cnt_freq * total_time_in_ns_, "CPU utilization");
  print_GHz_(cpu_cycles * cnt_freq, cnt_cycles * 1e9, "Average frequency");

  std::cout << "Breakdown based on instruction mix:\n";
  uint64_t group_id = 0;
  uint64_t inst_spec = get_schedulable_event_stat_by_name("inst_spec", group_id).total_value;
  uint64_t ld_spec = get_schedulable_event_stat_by_name("ld_spec", group_id).total_value;
  uint64_t st_spec = get_schedulable_event_stat_by_name("st_spec", group_id).total_value;
  uint64_t dp_spec = get_schedulable_event_stat_by_name("dp_spec", group_id).total_value;
  uint64_t vfp_spec = get_schedulable_event_stat_by_name("vfp_spec", group_id).total_value;
  uint64_t ase_spec = get_schedulable_event_stat_by_name("ase_spec", group_id).total_value;
  uint64_t br_immed_spec = get_schedulable_event_stat_by_name("br_immed_spec", group_id).total_value;
  uint64_t br_indirect_spec = get_schedulable_event_stat_by_name("br_indirect_spec", group_id).total_value;
  uint64_t br_return_spec = get_schedulable_event_stat_by_name("br_return_spec", group_id).total_value;

  print_percentage_(ld_spec, inst_spec, "Load");
  print_percentage_(st_spec, inst_spec, "Store");
  print_percentage_(dp_spec, inst_spec, "Integer data processing");
  print_percentage_(vfp_spec, inst_spec, "Floating point");
  print_percentage_(ase_spec, inst_spec, "Advanced SIMD");
  print_percentage_(br_immed_spec, inst_spec, "Immediate branch");
  print_percentage_(br_indirect_spec, inst_spec, "Indirect branch");
  print_percentage_(br_return_spec, inst_spec, "Return branch");

  std::cout << "Breakdown based on misses:\n";
  uint64_t l1d_cache_refill = get_schedulable_event_stat_by_name("l1d_cache_refill", group_id).total_value;
  uint64_t l1i_cache_refill = get_schedulable_event_stat_by_name("l1i_cache_refill", group_id).total_value;
  uint64_t l2d_cache_refill = get_schedulable_event_stat_by_name("l2d_cache_refill", group_id).total_value;
  uint64_t l1d_tlb_refill = get_schedulable_event_stat_by_name("l1d_tlb_refill", group_id).total_value;
  uint64_t l1i_tlb_refill = get_schedulable_event_stat_by_name("l1i_tlb_refill", group_id).total_value;
  uint64_t dtlb_walk = get_schedulable_event_stat_by_name("dtlb_walk", group_id).total_value;
  uint64_t itlb_walk = get_schedulable_event_stat_by_name("itlb_walk", group_id).total_value;

  uint64_t denominator = get_fixed_event_stat_by_name("inst_retired", group_id).total_value;

  std::cout << " Cache:\n";
  print_decimal_(l1d_cache_refill * 1000, denominator, "L1D cache MPKI");
  print_decimal_(l1i_cache_refill * 1000, denominator, "L1I cache MPKI");
  print_decimal_(l2d_cache_refill * 1000, denominator, "L2 cache MPKI");

  std::cout << " TLB:\n";
  print_decimal_(l1d_tlb_refill * 1000, denominator, "L1D TLB MPKI");
  print_decimal_(l1i_tlb_refill * 1000, denominator, "L1I TLB MPKI");
  print_decimal_(dtlb_walk * 1000, denominator, "DTLB walk PKI");
  print_decimal_(itlb_walk * 1000, denominator, "ITLB walk PKI");

  uint64_t br_mis_pred_retired = get_schedulable_event_stat_by_name("br_mis_pred_retired", group_id).total_value;
  denominator = get_fixed_event_stat_by_name("inst_retired", group_id).total_value;

  std::cout << " Branch predictor:\n";
  print_decimal_(br_mis_pred_retired * 1000, denominator, "Branch MPKI");

  std::cout << "Memory access latency:\n";
  uint64_t bus_access_rd = get_schedulable_event_stat_by_name("bus_access_rd", group_id).total_value;
  uint64_t bus_access_wr = get_schedulable_event_stat_by_name("bus_access_wr", group_id).total_value;
  uint64_t mem_access_rd = get_schedulable_event_stat_by_name("mem_access_rd", group_id).total_value;
  uint64_t bus_access_rd_cycles = get_schedulable_event_stat_by_name("bus_access_rd_cycles", group_id).total_value;
  uint64_t bus_access_wr_cycles = get_schedulable_event_stat_by_name("bus_access_wr_cycles", group_id).total_value;
  uint64_t mem_access_rd_cycles = get_schedulable_event_stat_by_name("mem_access_rd_cycles", group_id).total_value;

  uint64_t dtlb_walk_cycles = get_schedulable_event_stat_by_name("dtlb_walk_cycles", group_id).total_value;
  uint64_t itlb_walk_cycles = get_schedulable_event_stat_by_name("itlb_walk_cycles", group_id).total_value;

  print_cycles_(bus_access_rd_cycles, bus_access_rd, "Bus read latency");
  print_cycles_(bus_access_wr_cycles, bus_access_wr, "Bus write latency");
  print_cycles_(mem_access_rd_cycles, mem_access_rd, "Memory read latency");
  print_cycles_(dtlb_walk_cycles, dtlb_walk, "DTLB walk latency");
  print_cycles_(itlb_walk_cycles, itlb_walk, "ITLB walk latency");
}

void Reporter::print_metrics_cortex_x4_() {
  std::cout << "Pipeline basic metrics:\n";
  uint64_t cpu_cycles = get_fixed_event_stat_by_name("cpu_cycles", 0).estimated_value;
  uint64_t inst_retired = get_fixed_event_stat_by_name("inst_retired", 0).estimated_value;
  uint64_t cnt_cycles = get_fixed_event_stat_by_name("cnt_cycles", 0).estimated_value;
  uint64_t cnt_freq = read_cntfrq_el0();

  print_decimal_(cpu_cycles, inst_retired, "CPI");
  print_percentage_(cnt_cycles * 1e9, cnt_freq * total_time_in_ns_, "CPU utilization");
  print_GHz_(cpu_cycles * cnt_freq, cnt_cycles * 1e9, "Average frequency");

  std::cout << "Breakdown based on instruction mix:\n";
  uint64_t group_id = 0;
  uint64_t inst_spec = get_schedulable_event_stat_by_name("inst_spec", group_id).total_value;
  uint64_t ld_spec = get_schedulable_event_stat_by_name("ld_spec", group_id).total_value;
  uint64_t st_spec = get_schedulable_event_stat_by_name("st_spec", group_id).total_value;
  uint64_t dp_spec = get_schedulable_event_stat_by_name("dp_spec", group_id).total_value;
  uint64_t vfp_spec = get_schedulable_event_stat_by_name("vfp_spec", group_id).total_value;
  uint64_t ase_spec = get_schedulable_event_stat_by_name("ase_spec", group_id).total_value;
  uint64_t br_immed_spec = get_schedulable_event_stat_by_name("br_immed_spec", group_id).total_value;
  uint64_t br_indirect_spec = get_schedulable_event_stat_by_name("br_indirect_spec", group_id).total_value;
  uint64_t br_return_spec = get_schedulable_event_stat_by_name("br_return_spec", group_id).total_value;

  print_percentage_(ld_spec, inst_spec, "Load");
  print_percentage_(st_spec, inst_spec, "Store");
  print_percentage_(dp_spec, inst_spec, "Integer data processing");
  print_percentage_(vfp_spec, inst_spec, "Floating point");
  print_percentage_(ase_spec, inst_spec, "Advanced SIMD");
  print_percentage_(br_immed_spec, inst_spec, "Immediate branch");
  print_percentage_(br_indirect_spec, inst_spec, "Indirect branch");
  print_percentage_(br_return_spec, inst_spec, "Return branch");

  std::cout << "Breakdown based on misses:\n";
  uint64_t l1d_cache_refill = get_schedulable_event_stat_by_name("l1d_cache_refill", group_id).total_value;
  uint64_t l1i_cache_refill = get_schedulable_event_stat_by_name("l1i_cache_refill", group_id).total_value;
  uint64_t l2d_cache_refill = get_schedulable_event_stat_by_name("l2d_cache_refill", group_id).total_value;
  uint64_t l3d_cache_refill = get_schedulable_event_stat_by_name("l3d_cache_refill", group_id).total_value;
  uint64_t l1d_tlb_refill = get_schedulable_event_stat_by_name("l1d_tlb_refill", group_id).total_value;
  uint64_t l1i_tlb_refill = get_schedulable_event_stat_by_name("l1i_tlb_refill", group_id).total_value;
  uint64_t denominator = get_fixed_event_stat_by_name("inst_retired", group_id).total_value;

  std::cout << " Cache:\n";
  print_decimal_(l1d_cache_refill * 1000, denominator, "L1D cache MPKI");
  print_decimal_(l1i_cache_refill * 1000, denominator, "L1I cache MPKI");
  print_decimal_(l2d_cache_refill * 1000, denominator, "L2 cache MPKI");
  print_decimal_(l3d_cache_refill * 1000, denominator, "L3 cache MPKI");

  std::cout << " TLB:\n";
  print_decimal_(l1d_tlb_refill * 1000, denominator, "L1D TLB MPKI");
  print_decimal_(l1i_tlb_refill * 1000, denominator, "L1I TLB MPKI");

  uint64_t dtlb_walk = get_schedulable_event_stat_by_name("dtlb_walk", group_id).total_value;
  uint64_t itlb_walk = get_schedulable_event_stat_by_name("itlb_walk", group_id).total_value;
  denominator = get_fixed_event_stat_by_name("inst_retired", group_id).total_value;

  print_decimal_(dtlb_walk * 1000, denominator, "DTLB walk PKI");
  print_decimal_(itlb_walk * 1000, denominator, "ITLB walk PKI");

  std::cout << " Branch predictor:\n";
  uint64_t br_mis_pred_retired = get_schedulable_event_stat_by_name("br_mis_pred_retired", group_id).total_value;
  denominator = get_fixed_event_stat_by_name("inst_retired", group_id).total_value;

  print_decimal_(br_mis_pred_retired * 1000, denominator, "Branch MPKI");

  std::cout << "Memory access latency:\n";
  uint64_t mem_access_rd = get_schedulable_event_stat_by_name("mem_access_rd", group_id).total_value;
  uint64_t mem_access_rd_percyc = get_schedulable_event_stat_by_name("mem_access_rd_percyc", group_id).total_value;
  uint64_t dtlb_walk_percyc = get_schedulable_event_stat_by_name("dtlb_walk_percyc", group_id).total_value;
  uint64_t itlb_walk_percyc = get_schedulable_event_stat_by_name("itlb_walk_percyc", group_id).total_value;

  print_cycles_(mem_access_rd_percyc, mem_access_rd, "Memory read latency");
  print_cycles_(dtlb_walk_percyc, dtlb_walk, "DTLB walk latency");
  print_cycles_(itlb_walk_percyc, itlb_walk, "ITLB walk latency");
}

void Reporter::print_event_count_(uint64_t c, std::string event_name) {
  std::cout << "  " << std::left << std::setw(22) << event_name
            << std::right << std::setw(20) << format_with_commas_(c) << '\n';
}

void Reporter::print_percentage_(uint64_t a, uint64_t b, std::string metric_name) {
  double pct = (b > 0) ? (double)a / b * 100 : 0.0;
  std::cout << "  " << std::left << std::setw(27) << metric_name
            << std::right << std::setw(13) << std::fixed << std::setprecision(2) << pct << " \%\n";
}

void Reporter::print_decimal_(uint64_t a, uint64_t b, std::string metric_name) {
  double dcml = (b > 0) ? (double)a / b : 0.0;
  std::cout << "  " << std::left << std::setw(30) << metric_name
            << std::right << std::setw(12) << std::fixed << std::setprecision(4) << dcml << '\n';
}

void Reporter::print_cycles_(uint64_t a, uint64_t b, std::string metric_name) {
  double cyc = (b > 0) ? (double)a / b : 0.0;
  std::cout << "  " << std::left << std::setw(23) << metric_name
            << std::right << std::setw(12) << std::fixed << std::setprecision(4) << cyc << " cycles\n";
}

void Reporter::print_GHz_(uint64_t a, uint64_t b, std::string metric_name) {
  double freq_in_GHz = (b > 0) ? (double)a / b : 0.0;
  std::cout << "  " << std::left << std::setw(22) << metric_name
            << std::right << std::setw(16) << std::fixed << std::setprecision(4) << freq_in_GHz << " GHz\n";
}