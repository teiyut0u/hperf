#pragma once

#include <string>
#include <cstdint>

struct PMUEvent {
  std::string name;
  std::string description;
  uint64_t encoding;
};
