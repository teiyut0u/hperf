#ifndef PERF_EVENT_ATTR
#define PERF_EVENT_ATTR

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
 * @note Common settings are privide, and you can custom by getting the pointer.
 */
class PerfEventAttr {
 public:
  PerfEventAttr();

  /**
   * @brief Get the pointer to `struct perf_event_attr`.
   *
   * @return Pointer to `struct perf_event_attr`.
   *
   * @note `struct perf_event_attr` is managed and owned by the object. You just mofify its field or copy it.
   */
  struct perf_event_attr* get() { return &attr; }

  /**
   * @brief Set type the value specified by `type_path`.
   *
   * @param `type_path` The filesystem interface that provides the type value.
   *
   * @return The error if error happens, else `std::errc()` to represent no error. (Note: value of `std::errc()` is 0.)
   *
   * @see https://man7.org/linux/man-pages/man2/perf_event_open.2.html 'perf_event related configuration files' section
   */
  std::error_code set_type(const fs::path& type_path);

  /**
   * @brief Set `type`.
   *
   * @param `type_val` The value to set to.
   */
  void set_type(uint32_t type_val) { attr.type = type_val; }

  /**
   * @brief Add event to `struct perf_event_attr` 's config.
   *
   * @param `event_path` The filesystem interface that provides the config of the event.
   *
   * @return The error if error happens, else `std::errc()` to represent no error. (Note: value of `std::errc()` is 0.)
   *
   * @see https://man7.org/linux/man-pages/man2/perf_event_open.2.html 'perf_event related configuration files' section
   */
  std::error_code add_event(const fs::path& event_path);

  /**
   * @brief Add field to `struct perf_event_attr` 's config.
   *
   * @param `field_path` The filesystem interface that provides the config of the field.
   *
   * @param `field_val` The value of the field
   *
   * @return The error if error happens, else `std::errc()` to represent no error. (Note: value of `std::errc()` is 0.)
   *
   * @see https://man7.org/linux/man-pages/man2/perf_event_open.2.html 'perf_event related configuration files' section
   */
  std::error_code add_field(const fs::path& field_path, uint64_t field_val);

  /**
   * @brief Set the `disabled`.
   *
   * @param `disabled` The value to set to.
   */
  void set_disabled(uint64_t disabled = 1) { attr.disabled = disabled; }

  /**
   * @brief Set the `read_format`.
   *
   * @param `read_format` The value to set to.
   */
  void set_read_format(uint64_t read_format) { attr.read_format = read_format; }

  /**
   * @brief Parse `struct perf_event_attr` from string.
   *
   * @param `event_str` The event string in format: device_name/event_name,param1=val,.../.
   *
   * @return The error if error happens, else `std::errc()` to represent no error. (Note: value of `std::errc()` is 0.)
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

  struct perf_event_attr attr;
};

#endif
