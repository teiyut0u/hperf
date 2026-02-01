#include "hperf/util/hperf_error.hpp"

#include <utility>

hperf::HperfError::HperfError() {}

hperf::HperfError::HperfError(std::string message) : message_(std::move(message)) {}

const std::string& hperf::HperfError::get_message() {
  return message_;
}

void hperf::HperfError::set_message(std::string message) {
  this->message_ = std::move(message);
}
