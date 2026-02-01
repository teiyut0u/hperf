#ifndef HPERF_ERROR_HPP
#define HPERF_ERROR_HPP

#include <string>

namespace hperf {

class HperfError {
 public:
  HperfError();
  explicit HperfError(std::string message);

  const std::string& get_message();
  void set_message(std::string message);

 private:
  std::string message_;
};

}  // namespace hperf

#endif
