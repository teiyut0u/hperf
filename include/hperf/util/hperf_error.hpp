#ifndef HPERF_ERROR_HPP
#define HPERF_ERROR_HPP

#include <string>

namespace hperf {

class HperfError {
 public:
  HperfError();
  explicit HperfError(const std::string& message);
  explicit HperfError(std::string&& message);
  HperfError(HperfError&& another);
  HperfError& operator=(HperfError&& another);
  explicit operator bool() const { return this->has_error_; }
  const std::string& message();
  // void set_message(std::string&& message);

 private:
  std::string message_;
  bool has_error_;
};

}  // namespace hperf

#endif
