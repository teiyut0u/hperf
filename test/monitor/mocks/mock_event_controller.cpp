#include "mocks/mock_event_controller.hpp"

MockEventController::MockEventController() = default;

std::error_code MockEventController::control(unsigned long request, void* arg) const {
  control_call_count_++;
  if (control_error_) {
    return control_error_;
  }
  return std::error_code{};
}

std::error_code MockEventController::read() {
  read_call_count_++;
  if (read_error_) {
    return read_error_;
  }
  return std::error_code{};
}

std::error_code MockEventController::close() {
  close_call_count_++;
  if (close_error_) {
    return close_error_;
  }
  return std::error_code{};
}
