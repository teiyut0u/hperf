#include "hperf/util/hperf_util.hpp"

#include <linux/perf_event.h>
#include <sys/stat.h>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "hperf/util/hperf_error.hpp"
#include "hperf/util/hperf_result.hpp"
#include "hperf/util/hperf_util.hpp"

namespace fs = std::filesystem;

std::unique_ptr<struct perf_event_attr> hperf::make_empty_attr() {
  std::unique_ptr<struct perf_event_attr> result_ptr = std::make_unique<struct perf_event_attr>(perf_event_attr{});
  std::memset(result_ptr.get(), 0, sizeof(perf_event_attr));
  return result_ptr;
}

std::unique_ptr<struct perf_event_attr> hperf::make_attr() {
  auto result_ptr = std::make_unique<struct perf_event_attr>(perf_event_attr{});
  std::memset(result_ptr.get(), 0, sizeof(perf_event_attr));
  result_ptr->size = sizeof(struct perf_event_attr);
  result_ptr->disabled = 1;
  return result_ptr;
}

bool retry_fread(std::string& result, FILE* stream, unsigned int retry_times) {
  char buffer[4096];
  for (int attempt = 0; attempt <= retry_times; ++attempt) {
    if (attempt > 0) {
      // clear before retry
      clearerr(stream);
      result.clear();
    }

    size_t n;
    while ((n = fread(buffer, sizeof(char), sizeof(buffer), stream)) > 0) {
      result.append(buffer, n);
    }

    if (!ferror(stream)) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Read a file and return its context as string. Sometimes the error is recoverable, so you can retry.
 *
 * @param file_path The path of the file to read.
 * @param retry_times Specify the retry times. Will try at most retry_times+1 times in total(1 try and retry_times retry). Will not retry by default.
 *
 * @return Return result in string if success, else return error.
 */
hperf::Result<std::string, std::error_code> hperf::read_file(const fs::path& file_path, unsigned int retry_times) {
  // open file
  FILE* fp = fopen(file_path.c_str(), "rb");
  // return error if failed to open
  if (!fp) {
    return hperf::make_error_result<std::string, std::error_code>(
        errno, std::generic_category());
  }
  // check file state
  int fd = fileno(fp);
  struct stat st;
  if (fstat(fd, &st) != 0) {
    int saved_errno = errno;
    fclose(fp);
    return hperf::make_error_result<std::string, std::error_code>(
        saved_errno, std::generic_category());
  }
  // this function read regular file
  if (!S_ISREG(st.st_mode)) {
    fclose(fp);
    if (S_ISDIR(st.st_mode)) {
      return hperf::make_error_result<std::string, std::error_code>(
          std::move(std::make_error_code(std::errc::is_a_directory)));
    }
    return hperf::make_error_result<std::string, std::error_code>(
        std::move(std::make_error_code(std::errc::invalid_argument)));
  }
  // read file
  std::string result;
  if (retry_fread(result, fp, retry_times)) {
    fclose(fp);
    return hperf::make_result<std::string, std::error_code>(std::move(result));
  } else {
    fclose(fp);
    return hperf::make_error_result<std::string, std::error_code>(
        std::move(std::make_error_code(std::errc::io_error)));
  }
}

std::error_code hperf::add_type(const fs::path& type_path, struct perf_event_attr& attr) {
  auto type_result = hperf::read_file(type_path);
  if (type_result.ok()) {
    auto type_str_ptr = std::move(type_result).get_result_ptr();
    auto [ptr, ec] = std::from_chars(
        type_str_ptr->data(),
        type_str_ptr->data() + type_str_ptr->size(),
        attr.type);
    if (ec != std::errc()) {
      return std::make_error_code(ec);
    }
  } else {
    return *std::move(type_result).get_error_ptr();
  }
  return std::error_code{};
}

std::error_code hperf::add_field(const fs::path& field_path, uint64_t field_val, struct perf_event_attr& attr) {
  // printf("add %s 0x%016lx\n", field_path.string().c_str(), field_val);
  std::string buffer;
  auto field_config = hperf::read_file(field_path);
  if (field_config.ok()) {
    buffer = *std::move(field_config).get_result_ptr();
  } else {
    return *std::move(field_config).get_error_ptr();
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
    auto [start_unparsed_ptr, start_ec] =
        std::from_chars(buffer.data() + start_parse_position,
                        buffer.data() + next_to_sign_position - start_parse_position,
                        start_bit);
    if (start_ec == std::errc::invalid_argument) {
      return std::make_error_code(start_ec);
    }
    auto [end_unparsed_ptr, end_ec] =
        std::from_chars(buffer.data() + second_num_start_position,
                        buffer.data() + next_comma_sign_position - second_num_start_position,
                        end_bit);
    if (end_ec == std::errc::invalid_argument) {
      return std::make_error_code(end_ec);
    }
    // add field.
    // A field may be seperated into several part, add one part each time
    uint8_t bits_length = end_bit - start_bit + 1;
    *config_ptr |=
        ((field_val >> accumulated_offset)  // remove the bits that have been added to config
         & ((1u << bits_length) - 1))       // mask, get the needed bits
        << start_bit;                       // move the bits to the correct position
    accumulated_offset += bits_length;
    start_parse_position = next_comma_sign_position + 1;
  }
  return std::error_code();
}

std::error_code hperf::add_event(const fs::path& event_path, struct perf_event_attr& attr) {
  std::unique_ptr<std::string> buffer_ptr;
  auto event_field_result = hperf::read_file(event_path);
  if (!event_field_result.ok()) {
    return *std::move(event_field_result).get_error_ptr();
  }
  buffer_ptr = std::move(event_field_result).get_result_ptr();
  auto params = parse_param_views(buffer_ptr->begin(), buffer_ptr->end());
  for (const auto& pair : params) {
    // std::cout << '"' << pair.first << "\" \"" << pair.second << '"' << std::endl;
    std::string field_name{pair.first};
    uint64_t field_val;
    auto parse_errc = hperf::string2integer(pair.second, field_val);
    if (parse_errc != std::errc()) {
      return std::make_error_code(parse_errc);
    }
    // auto [ptr, ec] = std::from_chars(pair.second.data(), pair.second.data() + pair.second.size(), field_val);
    // if (ec != std::errc()) {
    //   return hperf::HperfError{"Failed to parse the value of field '" + field_name +
    //                            "', the value string is \"" + std::string(pair.second) + '"'};
    // }
    auto it = std::find(event_path.begin(), event_path.end(), "devices");
    if (it == event_path.end() || ++it == event_path.end()) {
      return std::make_error_code(std::errc::invalid_argument);
    }
    auto field_path = hperf::DEVICES_DIR / (*it) / "format" / pair.first;
    // printf("%s %lu\n", field_path.string().c_str(), field_val);
    auto add_field_error_code = hperf::add_field(field_path, field_val, attr);
    if (add_field_error_code) {
      return add_field_error_code;
    }
  }
  return {};
}

inline static void parse_param_views_helper(
    std::string::const_iterator start_parse_it,
    std::string::const_iterator pre_equal_it,
    std::string::const_iterator current_it,
    std::vector<std::pair<std::string_view, std::string_view>>& result) {
  auto start_to_equal = std::distance(start_parse_it, pre_equal_it);
  if (start_to_equal > 0) {
    result.emplace_back(std::string_view{&*start_parse_it,
                                         static_cast<size_t>(start_to_equal)},
                        std::string_view{&*(pre_equal_it + 1),
                                         static_cast<size_t>(std::distance(pre_equal_it + 1, current_it))});
  } else {
    result.emplace_back(std::string_view{&*start_parse_it,
                                         static_cast<size_t>(std::distance(start_parse_it, current_it))},
                        std::string_view{});
  }
}

std::vector<std::pair<std::string_view, std::string_view>> hperf::parse_param_views(
    std::string::const_iterator begin_it,
    std::string::const_iterator end_it) {
  std::vector<std::pair<std::string_view, std::string_view>> result;
  std::string::const_iterator start_parse_it = begin_it,
                              pre_equal_it = begin_it;
  for (auto it = begin_it; it != end_it; ++it) {
    if (*it == '=') {
      pre_equal_it = it;
    } else if (*it == ',') {
      parse_param_views_helper(start_parse_it, pre_equal_it, it, result);
      start_parse_it = it + 1;
    }
  }
  parse_param_views_helper(start_parse_it, pre_equal_it, end_it, result);
  return result;
}

static hperf::HperfError parse_param(const std::string& event_str, struct perf_event_attr& attr) {
  // add type
  auto param_left_border = event_str.find('/');
  fs::path device_path = hperf::DEVICES_DIR / event_str.substr(0, param_left_border);
  auto add_type_error = hperf::add_type(device_path / "type", attr);
  if (add_type_error) {
    return hperf::HperfError{add_type_error.message()};
  }

  auto params = hperf::parse_param_views(event_str.begin() + param_left_border + 1,
                                         event_str.end() - 1);
  for (const auto& pair : params) {
    if (pair.second.empty()) {
      auto add_event_err = hperf::add_event(device_path / "events" / pair.first, attr);
      if (add_event_err) {
        return hperf::HperfError{add_event_err.message()};
      }
    } else {
      uint64_t field_val;
      auto errc = hperf::string2integer(pair.second, field_val);
      if (errc != std::errc()) {
        return hperf::HperfError{std::make_error_code(errc).message()};
      }
      auto add_field_err = hperf::add_field(device_path / "format" / pair.first, field_val, attr);
    }
  }

  // size_t start_parse_position = param_left_border + 1;
  // while (start_parse_position < event_str.size()) {
  //   size_t next_comma_position = event_str.find(',', start_parse_position);
  //   if (next_comma_position == std::string::npos) {
  //     next_comma_position = event_str.size() - 1;
  //   }
  //   size_t next_equal_sign_position = event_str.find('=', start_parse_position);
  //   // parse the name and value of the field
  //   std::string field_name;
  //   uint64_t field_val;
  //   if (next_equal_sign_position >= next_comma_position) {
  //     // in this case, the param just contain the event name
  //     auto event_path = device_path / "events" / event_str.substr(start_parse_position, next_comma_position - start_parse_position);
  //     auto event_result = hperf::read_file(event_path);
  //     if (event_result.ok()) {
  //       std::string tmp_str = *std::move(event_result).get_result_ptr();
  //       // printf("result is '%s' %lu\n", tmp_str.c_str(), tmp_str.size());
  //       hperf::HperfError parse_error = parse_param(
  //           event_str.substr(0, param_left_border + 1) + tmp_str + "/",
  //           attr);
  //       if (parse_error) {
  //         return parse_error;
  //       }
  //     } else {
  //       return hperf::HperfError{"Failed to get event field '" + fs::absolute(event_path).string() + "' because:\n" + std::move(event_result).get_error_ptr()->message()};
  //     }
  //   } else {
  //     // in this case, it should be 'param=value'
  //     field_name = event_str.substr(start_parse_position, next_equal_sign_position - start_parse_position);
  //     // printf("%lu %lu %lu\n", next_equal_sign_position, next_comma_position, event_str.size());
  //     // printf("%s '%s'\n", event_str.c_str(), event_str.substr(next_equal_sign_position + 1, next_comma_position - next_equal_sign_position - 1).c_str());
  //     auto field_val_str = event_str.substr(next_equal_sign_position + 1, next_comma_position - next_equal_sign_position - 1);
  //     try {
  //       field_val = std::stoi(field_val_str, nullptr, 0);
  //     } catch (const std::exception& e) {
  //       return hperf::HperfError{"Failed to parse the value of field '" + fs::absolute(device_path / "format" / field_name).string() + "' because:\nstoi get invalid argument: \"" + field_val_str + '"'};
  //     }
  //     hperf::HperfError add_field_error = hperf::add_field(device_path / "format" / field_name, field_val, attr);
  //     if (add_field_error) {
  //       return add_field_error;
  //     }
  //   }
  //   start_parse_position = next_comma_position + 1;
  // }

  return hperf::HperfError{};
}

hperf::HperfError hperf::parse_event_from(const std::string& event_str, struct perf_event_attr& attr) {
  // check whether the event string is right format
  // only device/param1=...,param2,.../ is avaliable now
  // maybe will be compatible to perf later
  auto param_left_border = event_str.find('/');
  if (param_left_border == std::string::npos || event_str.find('/', param_left_border + 1) != event_str.size() - 1) {
    return hperf::HperfError{"The event string is invalid"};
  }

  // parse
  return parse_param(event_str, attr);
}
