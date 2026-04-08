#pragma once

#include "hperf/profile_config.h"

/**
 * @brief Entry point for CMN memory bandwidth monitoring.
 *
 * Reads memory controller positions from file, sets up CMN watchpoint events,
 * and runs a monitoring loop that outputs bandwidth data at regular intervals.
 *
 * @param profile_config Profiling configuration (monitor_target, mc_position_file, etc.)
 * @return true on success, false on error (error messages printed to stderr)
 */
bool cmn_bandwidth_monitor(const ProfileConfig& profile_config);
