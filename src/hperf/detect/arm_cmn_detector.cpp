#include "hperf/detect/arm_cmn_detector.hpp"

#include <linux/perf_event.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

#include "hperf/util/event_controller/single_event_controller.hpp"
#include "hperf/util/hperf_cache.hpp"
#include "hperf/util/hperf_error.hpp"
#include "hperf/util/hperf_result.hpp"
#include "hperf/util/hperf_util.hpp"

size_t hperf::ArmCmnDetector::detect_devices() {
  size_t result = 0;
  for (const auto& entry : std::filesystem::directory_iterator(hperf::DEVICES_DIR)) {
    auto device_name = entry.path().filename().string();
    if (device_name.compare(0, 8, "arm_cmn_") == 0 &&
        std::all_of(device_name.begin() + 8, device_name.end(), ::isdigit)) {
      ++result;
    }
  }
  // this->device_count_ = result;
  this->arm_cmns_.resize(result);
  return result;
}

hperf::Result<bool, std::error_code> hperf::ArmCmnDetector::test_xp(size_t device_id, uint16_t nodeid) {
  auto test_attr_result = this->make_test_xp_attr(device_id, nodeid);
  if (test_attr_result.ok()) {
    int fd = perf_event_open(std::move(test_attr_result).get_result_ptr().get(), -1, -1, -1, 0);
    if (fd != -1) {
      return make_result<bool, std::error_code>(true);
    }
    // these errors means not supported.
    // if errno is not within them, it's error
    if (errno != EINVAL && errno != ENOSYS && errno != ENOENT && errno != EOPNOTSUPP) {
      return make_error_result<bool, std::error_code>(errno, std::generic_category());
    } else {
      return make_result<bool, std::error_code>(false);
    }
  } else {
    return hperf::Result<bool, std::error_code>{std::move(test_attr_result).get_error_ptr()};
  }
}

hperf::Result<size_t, std::error_code> hperf::ArmCmnDetector::test_nodeid_bits_size_helper(size_t device_id, uint64_t nodeid_start, uint16_t nodeid_to, uint16_t nodeid_step, bool target_sign, uint target_val) {
  for (auto nodeid = nodeid_start; nodeid <= nodeid_to; nodeid += nodeid_step) {
    auto test_xp_result = this->test_xp(device_id, nodeid);
    if (test_xp_result.ok()) {
      if (*std::move(test_xp_result).get_result_ptr() == target_sign) {
        return make_result<size_t, std::error_code>(target_val);
      }
    } else {
      return hperf::make_error_result<size_t, std::error_code>(std::move(test_xp_result).get_error_ptr());
    }
  }
  return hperf::Result<size_t, std::error_code>{0};
}

hperf::Result<size_t, std::error_code> hperf::ArmCmnDetector::test_nodeid_bits_size(size_t device_id) {
  // try cache
  auto nodeid_bits_size_cache_target = this->get_device_name(device_id) + "/nodeid_bits_size";
  size_t nodeid_bits_size;
  // if read successfully, return cache
  if (hperf::HperfCache::read_cache(nodeid_bits_size_cache_target, &nodeid_bits_size, sizeof(nodeid_bits_size)) == std::errc()) {
    return hperf::make_result<size_t, std::error_code>(nodeid_bits_size);
  }

#define TEST_NODEID(device_identify, nodeid_start, nodeid_to, nodeid_step, target_sign, target_val) \
  {                                                                                                 \
    auto test_result = this->test_nodeid_bits_size_helper(                                          \
        device_identify, nodeid_start, nodeid_to, nodeid_step, target_sign, target_val);            \
    if (test_result.ok()) {                                                                         \
      auto nodeid_bits_size_ptr = std::move(test_result).get_result_ptr();                          \
      if (*nodeid_bits_size_ptr != 0) {                                                             \
        auto neglated = hperf::HperfCache::write_cache(                                             \
            nodeid_bits_size_cache_target,                                                          \
            nodeid_bits_size_ptr.get(),                                                             \
            sizeof(*nodeid_bits_size_ptr));                                                         \
        return hperf::Result<size_t, std::error_code>{*nodeid_bits_size_ptr};                       \
      }                                                                                             \
    } else {                                                                                        \
      return std::move(test_result);                                                                \
    }                                                                                               \
  }

  // if successed, it's 11 bits. else is 9 or 7 bits
  TEST_NODEID(device_id, 0x200, 0x200, 1, true, 11)
  // if successed, it's 9 bits
  // else, it may be 9(x<2 and y>3) or 7 bits, x and y start with 0
  TEST_NODEID(device_id, 0x80, 0x80, 1, true, 9)
  // test 0x0 to 0x20 step 0x8
  // if it's 9, since x<2 and y>3, all will pass
  // if it's 7, all pass only when x>0 and y=3
  TEST_NODEID(device_id, 0x0, 0x20, 0x8, false, 7)
  // test 0x28 to 0x38 step 0x8
  // if it's 7, since x>0 and y=3, all will pass
  // if it's 9, all pass only when x<2 and y=7
  TEST_NODEID(device_id, 0x28, 0x38, 0x8, false, 9)
  // find that bits in [4:0] don't matter any more, set them 0
  // since [15:7] is 0, the difference is in [6:5]
  // [6]=1 for 0 will always pass
  // so, test 0x40 and 0x60
  // if it's 9, since x<2 and y=7, all will pass
  // if it's 7, all pass only when x>0 and y=3
  TEST_NODEID(device_id, 0x40, 0x60, 0x20, false, 7)
#undef TEST_NODEID

  // there is no difference now, pick either case
  // the last test can be omitted it 7 is picked
  return hperf::make_result<size_t, std::error_code>(7);
}

hperf::Result<uint8_t, std::error_code> hperf::ArmCmnDetector::test_cmn_size_helper(size_t device_id, size_t nodeid_bits_size, size_t target_offset) {
  size_t left, right;
  for (left = 0, right = 1 << nodeid_bits_size; left < right;) {
    auto mid = (left + right) >> 1;
    uint16_t nodeid = mid << target_offset;
    auto xp_result = this->test_xp(device_id, nodeid);
    if (!xp_result.ok()) {
      return hperf::make_error_result<uint8_t, std::error_code>(
          std::move(xp_result).get_error_ptr());
    }
    if (*std::move(xp_result).get_result_ptr() == true) {
      left = mid + 1;
    } else {
      right = mid - 1;
    }
  }
  auto xp_result = this->test_xp(device_id, left << target_offset);
  if (!xp_result.ok()) {
    return hperf::make_error_result<uint8_t, std::error_code>(
        std::move(xp_result).get_error_ptr());
  }
  if (*std::move(xp_result).get_result_ptr() == true) {
    return hperf::make_result<uint8_t, std::error_code>(left);
  } else {
    return hperf::make_result<uint8_t, std::error_code>(left - 1);
  }
}

hperf::Result<std::pair<uint8_t, uint8_t>, std::error_code> hperf::ArmCmnDetector::test_cmn_size(size_t device_id) {
  // try cache
  auto cmn_size_cache_target = this->get_device_name(device_id) + "/cmn_size";
  auto cmn_size_cache_ptr = std::make_unique<std::pair<uint8_t, uint8_t>>();
  if (hperf::HperfCache::read_cache(
          cmn_size_cache_target,
          cmn_size_cache_ptr.get(),
          sizeof(*cmn_size_cache_ptr)) == std::errc()) {
    return hperf::make_result<std::pair<uint8_t, uint8_t>, std::error_code>(
        std::move(cmn_size_cache_ptr));
  }
  // get nodeid_bits_size
  auto nodeid_bits_size_result = this->test_nodeid_bits_size(device_id);
  if (!nodeid_bits_size_result.ok()) {
    return hperf::make_error_result<std::pair<uint8_t, uint8_t>, std::error_code>(
        std::move(nodeid_bits_size_result).get_error_ptr());
  }
  size_t nodeid_bits_size = *std::move(nodeid_bits_size_result).get_result_ptr();
  // test x
  auto x_result = this->test_cmn_size_helper(device_id, nodeid_bits_size, nodeid_bits_size + 3);
  if (x_result.ok()) {
    cmn_size_cache_ptr->first = *std::move(x_result).get_result_ptr();
  } else {
    return hperf::make_error_result<std::pair<uint8_t, uint8_t>, std::error_code>(
        std::move(x_result).get_error_ptr());
  }
  // test y
  auto y_result = this->test_cmn_size_helper(device_id, nodeid_bits_size, nodeid_bits_size + 3);
  if (y_result.ok()) {
    cmn_size_cache_ptr->second = *std::move(y_result).get_result_ptr();
  } else {
    return hperf::make_error_result<std::pair<uint8_t, uint8_t>, std::error_code>(
        std::move(y_result).get_error_ptr());
  }

  return hperf::make_result<std::pair<uint8_t, uint8_t>, std::error_code>(
      std::move(cmn_size_cache_ptr));
}

std::string hperf::ArmCmnDetector::get_device_name(size_t device_id) const {
  char buffer[20];
  // suppose it won't err
  auto [ptr, ec] = std::to_chars(buffer, buffer + sizeof(buffer), device_id);
  return std::string("arm_cmn_").append(buffer, ptr - buffer);
}

hperf::Result<struct perf_event_attr, std::error_code>
hperf::ArmCmnDetector::make_test_xp_attr(size_t device_id, uint16_t nodeid) const {
#define ARBITRAY_EVENT "mxp_e_dat_txflit_valid"
  auto attr_ptr = hperf::make_empty_attr();
  attr_ptr->size = sizeof(struct perf_event_attr);
  attr_ptr->disabled = 1;
  auto device_path = hperf::DEVICES_DIR / this->get_device_name(device_id);
  auto add_type_err = hperf::add_type(device_path / "type", *attr_ptr);
  if (add_type_err) {
    return hperf::make_error_result<struct perf_event_attr, std::error_code>(std::move(add_type_err));
  }
  auto add_event_error_code = hperf::add_event(device_path / "events" / ARBITRAY_EVENT, *attr_ptr);
  if (add_event_error_code) {
    return hperf::make_error_result<struct perf_event_attr, std::error_code>(add_event_error_code);
  }
  std::array<std::pair<std::string_view, uint64_t>, 2> params{{
      {"bynodeid", 0x1},
      {"nodeid", nodeid},
  }};
  for (const auto& pair : params) {
    auto add_field_error_code = add_field(device_path / "format" / pair.first, pair.second, *attr_ptr);
    if (add_field_error_code) {
      return hperf::make_error_result<struct perf_event_attr, std::error_code>(add_field_error_code);
    }
  }
  return hperf::Result<struct perf_event_attr, std::error_code>{std::move(attr_ptr)};
#undef ARBITRAY_EVENT
}
