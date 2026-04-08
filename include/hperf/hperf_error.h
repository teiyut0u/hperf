#pragma once

#include <stdexcept>

/**
 * @brief Base exception for all hperf errors.
 *
 * Thrown by initialization functions (e.g., PMUConfig::load_from_file,
 * EventScheduler::initialize) when a non-recoverable failure occurs.
 * Caught at the top level in main() which prints the message and exits.
 */
class HperfError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};
