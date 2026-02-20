#ifndef ARM_CMN_HPP
#define ARM_CMN_HPP

#include <cstdint>
#include <utility>

namespace hperf {

struct TestedArmCmn {
  uint8_t nodeid_bits_size;
  std::pair<uint8_t, uint8_t> cmn_size;
};

}  // namespace hperf

#endif
