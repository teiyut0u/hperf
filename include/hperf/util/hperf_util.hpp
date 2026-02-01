#ifndef HPERF_UTIL_HPP
#define HPERF_UTIL_HPP

#include <linux/perf_event.h>

#include <filesystem>
#include <memory>
#include <string>

#include "hperf/util/hperf_result.hpp"

namespace hperf {

namespace fs = std::filesystem;

hperf::HperfResult<std::string> read_file(const fs::path& file_path);

HperfResult<perf_event_attr> parse_event_from(const std::string& event_str);

}  // namespace hperf

#endif
