#ifndef PERF_EVENT_ATTR
#define PERF_EVENT_ATTR

#include <linux/perf_event.h>

#include <filesystem>
#include <string_view>
#include <system_error>
#include <vector>

namespace fs = std::filesystem;

class PerfEventAttr {
 public:
  PerfEventAttr();

  struct perf_event_attr* get() { return &attr; }

  std::error_code add_type(const fs::path& type_path);
  std::error_code add_event(const fs::path& event_path);
  std::error_code add_field(const fs::path& field_path, uint64_t field_val);
  void set_disabled(uint64_t disabled = 1) { attr.disabled = disabled; }
  void set_read_format(uint64_t read_format) { attr.read_format = read_format; }

  std::error_code parse_event_from(const std::string& event_str);

 private:
  std::vector<std::pair<std::string_view, std::string_view>>
  parse_param_views(std::string_view param_view);

  void parse_param_views_helper(
      std::string_view::const_iterator start_parse_it,
      std::string_view::const_iterator pre_equal_it,
      std::string_view::const_iterator current_it,
      std::vector<std::pair<std::string_view, std::string_view>>& result);

  std::error_code parse_param(const std::string& event_str);

  struct perf_event_attr attr;
};

#endif
