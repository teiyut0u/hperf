#ifndef ARM_CMN_DETECTOR_HPP
#define ARM_CMN_DETECTOR_HPP

#include <linux/perf_event.h>

#include <cstddef>
#include <cstdint>
#include <system_error>
#include <utility>
#include <vector>

#include "hperf/util/devices/arm_cmn.hpp"
#include "hperf/util/hperf_error.hpp"
#include "hperf/util/hperf_result.hpp"

namespace hperf {

class ArmCmnDetector {
 public:
  size_t detect_devices();
  hperf::Result<size_t, std::error_code> test_nodeid_bits_size(size_t device_id);
  hperf::HperfError test_cmn_size();
  hperf::Result<std::pair<uint8_t, uint8_t>, std::error_code> test_cmn_size(size_t id);

  size_t get_devices_count() const;
  const std::vector<std::pair<uint8_t, uint8_t>>& get_cmn_size() const;
  const std::pair<uint8_t, uint8_t>& get_cmn_size(size_t device_id) const;

 private:
  // size_t device_count_;
  std::vector<hperf::TestedArmCmn> arm_cmns_;
  hperf::Result<size_t, std::error_code> test_nodeid_bits_size_helper(size_t device_id, uint64_t nodeid_start, uint16_t nodeid_to, uint16_t nodeid_step, bool target_sign, uint target_val);
  hperf::Result<uint8_t, std::error_code> test_cmn_size_helper(size_t device_id, size_t nodeid_bits_size, size_t target_offset);
  std::string get_device_name(size_t device_id) const;
  hperf::Result<bool, std::error_code> test_xp(size_t device_id, uint16_t nodeid);
  // this attr is only used to test weather a xp is avaliable;
  hperf::Result<struct perf_event_attr, std::error_code>
  make_test_xp_attr(size_t device_id, uint16_t nodeid) const;
};

}  // namespace hperf

#endif
