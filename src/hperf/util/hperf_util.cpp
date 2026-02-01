#include "hperf/util/hperf_util.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include "hperf/util/hperf_error.hpp"
#include "hperf/util/hperf_result.hpp"
#include "hperf/util/hperf_util.hpp"

namespace fs = std::filesystem;

hperf::HperfResult<std::string> hperf::read_file(const fs::path& file_path) {
  // check whether the path exists
  if (!fs::exists(file_path)) {
    return hperf::make_error_result<std::string>("File not found: " + fs::absolute(file_path).string());
  }
  // read the path
  std::ifstream input_file(file_path);
  if (!input_file.is_open()) {
    return hperf::make_error_result<std::string>("Failed to read file: " + fs::absolute(file_path).string());
  }
  std::string result{
      std::istreambuf_iterator<char>(input_file),
      std::istreambuf_iterator<char>()};
  input_file.close();
  return hperf::make_result<std::string>(std::move(result));
}

static hperf::HperfResult<std::nullopt_t> add_field(const fs::path& field_path, uint64_t field_val, perf_event_attr& attr) {
  std::string buffer;
  auto field_config = hperf::read_file(field_path);
  if (field_config.ok()) {
    buffer = *std::move(field_config).get_result();
  } else {
    return hperf::make_error_result<std::nullopt_t>("Failed to add field " + fs::absolute(field_path).string() + " because:\n" + std::move(field_config).get_error()->get_message());
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
           behind_to_sign_position = next_to_sign_position + 1;
    if (next_comma_sign_position == std::string::npos) {
      next_comma_sign_position = buffer.size();
    }
    uint8_t start_bit = std::stoi(buffer.substr(start_parse_position, next_to_sign_position - start_parse_position)),
            bits_length = std::stoi(buffer.substr(behind_to_sign_position, next_comma_sign_position - behind_to_sign_position)) + 1 - start_bit;
    *config_ptr |= ((field_val >> accumulated_offset)  // remove the bits that have been added to config
                    & ((1u << bits_length) - 1))       // mask, get the needed bits
                   << start_bit;                       // move the bits to the correct position
    accumulated_offset += bits_length;
    start_parse_position = next_comma_sign_position + 1;
  }
  return std::nullopt;
}

static hperf::HperfResult<perf_event_attr> parse_param(const std::string event_str, size_t param_left_border) {
  std::unique_ptr<perf_event_attr> result_ptr = std::make_unique<perf_event_attr>(perf_event_attr{});
  std::memset(result_ptr.get(), 0, sizeof(perf_event_attr));
  static const fs::path DEVICES_DIR{"/sys/bus/event_source/devices"};

  // check whether the device exists
  // if exists, get type
  // else return error message
  fs::path device_path = DEVICES_DIR / event_str.substr(0, param_left_border);
  auto type_result = hperf::read_file(device_path);
  if (type_result.ok()) {
    result_ptr->type = std::stoi(*std::move(type_result).get_result());
  } else {
    return hperf::make_error_result<perf_event_attr>("Failed to get event type because:\n" + std::move(type_result).get_error()->get_message());
  }

  size_t start_parse_position = param_left_border + 1;
  while (start_parse_position < event_str.size()) {
    size_t next_comma_position = event_str.find(',', start_parse_position);
    if (next_comma_position == std::string::npos) {
      next_comma_position = event_str.size() - 1;
    }
    size_t next_equal_sign_position = event_str.find('=', start_parse_position);
    // parse the name and value of the field
    std::string field_name;
    uint64_t field_val;
    if (next_equal_sign_position >= next_comma_position) {
      // in this case, the param just contain the event name
      std::string buffer;
      auto event_result = hperf::read_file(device_path / "events" / event_str.substr(start_parse_position, next_comma_position - start_parse_position));
      if (event_result.ok()) {
        buffer = *std::move(event_result).get_result();
      } else {
        return hperf::make_error_result<perf_event_attr>("Failed to get event field because:\n" + std::move(event_result).get_error()->get_message());
      }
      size_t buffer_equal_sign_position = buffer.find('=');
      field_name = buffer.substr(0, buffer_equal_sign_position);
      field_val = std::stoi(buffer.substr(buffer_equal_sign_position + 1, buffer.size() - buffer_equal_sign_position - 1));
    } else {
      // in this case, it should be 'param=value'
      field_name = event_str.substr(start_parse_position, next_equal_sign_position - start_parse_position);
      field_val = std::stoi(event_str.substr(next_equal_sign_position + 1, next_comma_position - next_equal_sign_position - 1));
    }
    auto add_field_result = add_field(device_path / "format" / field_name, field_val, *result_ptr);
    if (!add_field_result.ok()) {
      return hperf::make_error_result<perf_event_attr>(std::move(add_field_result).get_error()->get_message());
    }
    start_parse_position = next_comma_position + 1;
  }

  return hperf::make_result<perf_event_attr>(std::move(result_ptr));
}

hperf::HperfResult<perf_event_attr> hperf::parse_event_from(const std::string& event_str) {
  // check whether the event string is right format
  // only device/param1=...,param2,.../ is avaliable now
  // maybe will be compatible to perf later
  auto param_left_border = event_str.find('/');
  if (param_left_border == std::string::npos || event_str.find('/', param_left_border + 1) != event_str.size() - 1) {
    return hperf::make_error_result<perf_event_attr>("event string is invalid");
  }

  // parse
  return parse_param(event_str, param_left_border);
}
