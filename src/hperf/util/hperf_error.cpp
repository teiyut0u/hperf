#include "hperf/util/hperf_error.hpp"

#include <utility>

hperf::HperfError::HperfError() : has_error_(false) {}

hperf::HperfError::HperfError(const std::string& message) : message_(std::move(message)), has_error_(true) {}

hperf::HperfError::HperfError(std::string&& message) : message_(std::move(message)), has_error_(true) {}

hperf::HperfError::HperfError(HperfError&& another) {
  *this = std::move(another);
}

hperf::HperfError& hperf::HperfError::operator=(HperfError&& another) {
  this->message_ = std::move(another.message_);
  this->has_error_ = another.has_error_;
  return *this;
}

const std::string& hperf::HperfError::message() {
  return message_;
}

// void hperf::HperfError::set_message(std::string&& message) {
//   this->message_ = std::move(message);
//   this->has_error_ = true;
// }
