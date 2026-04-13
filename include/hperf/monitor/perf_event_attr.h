#pragma once

#include <linux/perf_event.h>

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

/**
 * @brief Wrapper of `struct perf_event_attr`.
 *
 * @note Common settings are provided, and you can customize by getting the pointer.
 */
class PerfEventAttr {
 public:
  PerfEventAttr();

  /**
   * @brief Get the pointer to `struct perf_event_attr`.
   */
  struct perf_event_attr* get() { return &attr_; }

  /**
   * @brief Set type from the sysfs type file.
   */
  std::error_code set_type(const fs::path& type_path);

  /**
   * @brief Set `type` directly.
   */
  void set_type(uint32_t type_val) { attr_.type = type_val; }

  /**
   * @brief Add event to config by reading the sysfs event file.
   */
  std::error_code add_event(const fs::path& event_path);

  /**
   * @brief Add field to config by reading the sysfs format file.
   */
  std::error_code add_field(const fs::path& field_path, uint64_t field_val);

  void set_disabled(uint64_t disabled = 1) { attr_.disabled = disabled; }

  void set_read_format(uint64_t read_format) { attr_.read_format = read_format; }

  /**
   * @brief Parse `struct perf_event_attr` from event string.
   *
   * @param event_str Event string in format: device_name/event_name,param1=val,.../.
   */
  std::error_code parse_attr_from(std::string_view event_str);

 private:
  std::vector<std::pair<std::string_view, std::string_view>>
  parse_param_views(std::string_view param_view);

  void parse_param_views_helper(
      std::string_view::const_iterator start_parse_it,
      std::string_view::const_iterator pre_equal_it,
      std::string_view::const_iterator current_it,
      std::vector<std::pair<std::string_view, std::string_view>>& result);

  std::error_code parse_param(std::string_view event_str);

  struct perf_event_attr attr_;
};
