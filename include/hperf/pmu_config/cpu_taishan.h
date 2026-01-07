// Oryon PMU events configuration
// Applicable SoC platforms: MT6989 (Dimensity 9300)
// Arm Cortex-X4 + Cortex A720
// 1 fixed counter + 12 programmable counters per core
// This file is included by pmu_config.h

#ifndef CPU_ORYON_CONFIG_H
#define CPU_ORYON_CONFIG_H

// For clangd code hinting
#ifndef PMU_CONFIG_H
#include <vector>
#include "hperf/pmu_event.h"
#endif
// End for clangd code hinting

const std::vector<PMUEvent> fixed_events = {
    {"cpu_cycles", "Cycle", 0x11},
    {"cnt_cycles", "Constant frequency cycles", 0x4004},
    {"inst_retired", "Instruction architecturally executed", 0x08}};

const std::vector<std::vector<PMUEvent>> event_groups = {
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

#endif  // CPU_ORYON_CONFIG_H