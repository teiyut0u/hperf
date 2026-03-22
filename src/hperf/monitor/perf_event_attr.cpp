#include "hperf/monitor/perf_event_attr.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

#include "hperf/monitor/monitor_util.hpp"

PerfEventAttr::PerfEventAttr() {
  ::memset(&attr, 0, sizeof(attr));
  attr.size = sizeof(attr);
}

std::error_code PerfEventAttr::set_type(const fs::path& type_path) {
  std::string type_result;
  auto read_err = MonitorUtil::read_file(type_result, type_path);
  if (read_err) {
    return read_err;
  }
  auto conv_errc = MonitorUtil::string2integer(type_result, attr.type);
  if (conv_errc != std::errc()) {
    return std::make_error_code(conv_errc);
  }
  return std::error_code{};
}

std::error_code PerfEventAttr::add_event(const fs::path& event_path) {
  // printf("event_path '%s'\n", event_path.string().c_str());
  auto device_name_it = std::find(event_path.begin(), event_path.end(), "devices");
  if (device_name_it == event_path.end() || ++device_name_it == event_path.end()) {
    return std::make_error_code(std::errc::invalid_argument);
  }

  std::string event_field_result;
  auto read_err = MonitorUtil::read_file(event_field_result, event_path);
  if (read_err) {
    return read_err;
  }
  auto params = parse_param_views(event_field_result);
  for (const auto& pair : params) {
    std::string field_name{pair.first};
    uint64_t field_val;
    auto parse_errc = MonitorUtil::string2integer(pair.second, field_val);
    if (parse_errc != std::errc()) {
      continue;
    }
    auto field_path = MonitorUtil::DEVICES_DIR / (*device_name_it) / "format" / pair.first;
    auto add_field_error_code = add_field(field_path, field_val);
    if (add_field_error_code) {
      return add_field_error_code;
    }
  }
  return {};
}

std::error_code PerfEventAttr::add_field(const fs::path& field_path, uint64_t field_val) {
  std::string buffer;
  auto read_code = MonitorUtil::read_file(buffer, field_path);
  if (read_code) {
    return read_code;
  }

  // find the corresponding config
  __u64* config_ptr;
  size_t start_parse_position = 8;
  switch (buffer[6]) {
    case ':':
      config_ptr = &attr.config;
      start_parse_position = 7;
      break;
    case '1':
      config_ptr = &attr.config1;
      break;
    case '2':
      config_ptr = &attr.config2;
      break;
    case '3':
      config_ptr = &attr.config3;
      break;
  }
  // add field bits to the config
  uint8_t accumulated_offset = 0;
  while (start_parse_position < buffer.size()) {
    size_t next_to_sign_position = buffer.find('-', start_parse_position),
           next_comma_sign_position = buffer.find(',', next_to_sign_position + 1),
           second_num_start_position;
    if (next_to_sign_position == std::string::npos) {
      // don't have next '-' means there is one bit
      // in this case, the first bits position and the second one is same
      next_to_sign_position = buffer.size();
      second_num_start_position = start_parse_position;
    } else {
      second_num_start_position = next_to_sign_position + 1;
    }
    if (next_comma_sign_position == std::string::npos) {
      next_comma_sign_position = buffer.size();
    }
    // parse the first bits position and the second one
    // return error when failed
    uint8_t start_bit, end_bit;
    auto start_ec = MonitorUtil::string2integer(
        {buffer.data() + start_parse_position, next_to_sign_position - start_parse_position},
        start_bit);
    if (start_ec != std::errc()) {
      return std::make_error_code(start_ec);
    }
    auto end_ec = MonitorUtil::string2integer(
        {buffer.data() + second_num_start_position, next_comma_sign_position - second_num_start_position},
        end_bit);
    if (start_ec != std::errc()) {
      return std::make_error_code(end_ec);
    }
    // add field.
    // A field may be seperated into several part, add one part each time
    uint8_t bits_length = end_bit - start_bit + 1;
    *config_ptr |=
        ((field_val >> accumulated_offset)                   // remove the bits that have been added to config
         & (UINT64_MAX >> std::max(0, (64 - bits_length))))  // mask, get the needed bits
        << start_bit;                                        // move the bits to the correct position
    accumulated_offset += bits_length;
    start_parse_position = next_comma_sign_position + 1;
  }
  return std::error_code();
}

std::vector<std::pair<std::string_view, std::string_view>>
PerfEventAttr::parse_param_views(std::string_view param_view) {
  std::vector<std::pair<std::string_view, std::string_view>> result;
  std::string_view::const_iterator start_parse_it = param_view.cbegin(),
                                   pre_equal_it = param_view.cbegin();
  for (auto it = param_view.cbegin(); it != param_view.cend(); ++it) {
    if (*it == '=') {
      pre_equal_it = it;
    } else if (*it == ',') {
      parse_param_views_helper(start_parse_it, pre_equal_it, it, result);
      start_parse_it = it + 1;
    }
  }
  parse_param_views_helper(start_parse_it, pre_equal_it, param_view.cend(), result);
  return result;
}

void PerfEventAttr::parse_param_views_helper(
    std::string_view::const_iterator start_parse_it,
    std::string_view::const_iterator pre_equal_it,
    std::string_view::const_iterator current_it,
    std::vector<std::pair<std::string_view, std::string_view>>& result) {
  auto start_to_equal = std::distance(start_parse_it, pre_equal_it);
  if (start_to_equal > 0) {
    result.emplace_back(std::string_view{start_parse_it,
                                         static_cast<size_t>(start_to_equal)},
                        std::string_view{pre_equal_it + 1,
                                         static_cast<size_t>(std::distance(pre_equal_it + 1, current_it))});
  } else {
    result.emplace_back(std::string_view{start_parse_it,
                                         static_cast<size_t>(std::distance(start_parse_it, current_it))},
                        std::string_view{});
  }
}

std::error_code PerfEventAttr::parse_param(std::string_view event_str) {
  // add type
  auto param_left_border = event_str.find('/');
  fs::path device_path = MonitorUtil::DEVICES_DIR / std::string_view{event_str.data(), param_left_border};
  auto add_type_error = set_type(device_path / "type");
  if (add_type_error) {
    return add_type_error;
  }

  // add event and param
  auto params = parse_param_views({event_str.data() + param_left_border + 1, event_str.size() - param_left_border - 2});
  for (const auto& pair : params) {
    if (pair.second.empty()) {
      auto add_event_err = add_event(device_path / "events" / pair.first);
      if (add_event_err) {
        return add_event_err;
      }
    } else {
      uint64_t field_val;
      auto errc = MonitorUtil::string2integer(pair.second, field_val);
      if (errc != std::errc()) {
        return std::make_error_code(errc);
      }
      auto add_field_err = add_field(device_path / "format" / pair.first, field_val);
      if (add_field_err) {
        return add_field_err;
      }
    }
  }
  return {};
}

std::error_code PerfEventAttr::parse_attr_from(std::string_view event_str) {
  // check whether the event string is right format
  // only device/param1=...,param2,.../ is avaliable now
  // maybe will be compatible to perf later
  auto param_left_border = event_str.find('/');
  if (param_left_border == std::string::npos || event_str.find('/', param_left_border + 1) != event_str.size() - 1) {
    return std::make_error_code(std::errc::invalid_argument);
  }

  // parse
  return parse_param(event_str);
}
