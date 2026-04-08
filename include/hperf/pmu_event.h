#pragma once

#include <cstdint>
#include <string>

struct PMUEvent {
  std::string name;
  std::string description;
  uint64_t encoding;
};
