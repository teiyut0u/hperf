#ifndef EVENT_CONTROLLER_INTERFACE_HPP
#define EVENT_CONTROLLER_INTERFACE_HPP

#include <linux/perf_event.h>

#include <cstdint>
#include <optional>
#include <system_error>
#include <vector>

class EventControllerInterface {
 public:
  virtual ~EventControllerInterface() = default;

  virtual std::error_code control(unsigned long request, void*) const = 0;
  inline std::error_code enable() const { return this->control(PERF_EVENT_IOC_ENABLE, nullptr); }
  inline std::error_code disable() const { return this->control(PERF_EVENT_IOC_DISABLE, nullptr); }
  inline std::error_code reset() const { return this->control(PERF_EVENT_IOC_RESET, nullptr); }

  virtual std::error_code read() = 0;
  virtual std::error_code close() = 0;

  virtual size_t size() const = 0;
  virtual std::vector<uint64_t> all_value() const = 0;
  virtual std::optional<std::vector<uint64_t>> all_time_enabled() const = 0;
  virtual std::optional<std::vector<uint64_t>> all_time_running() const = 0;
  virtual std::optional<std::vector<uint64_t>> all_id() const = 0;
  virtual std::optional<std::vector<uint64_t>> all_lost() const = 0;

  virtual uint64_t get_read_format() const = 0;
};

#endif
