#include "hperf/monitor/monitor_util.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "hperf/monitor/perf_event_attr.hpp"

namespace fs = std::filesystem;

namespace {

void write_text_file(const fs::path& path, const std::string& content) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path);
  REQUIRE(out.is_open());
  out << content;
}

fs::path make_temp_case_dir(const std::string& case_name) {
  auto dir = fs::temp_directory_path() / ("hperf_monitor_util_test_" + case_name);
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  REQUIRE_FALSE(ec);
  return dir;
}

}  // namespace

TEST_CASE("MonitorUtil::read_file", "[monitor_util]") {
  SECTION("reads regular file content") {
    auto dir = make_temp_case_dir("read_file_ok");
    auto file_path = dir / "sample.txt";
    write_text_file(file_path, "hello monitor util");

    std::string content;
    auto ec = MonitorUtil::read_file(content, file_path);

    REQUIRE_FALSE(ec);
    REQUIRE(content == "hello monitor util");
    std::error_code remove_ec;
    fs::remove_all(dir, remove_ec);
  }

  SECTION("returns is_a_directory for directory input") {
    auto dir = make_temp_case_dir("read_file_dir");

    std::string content;
    auto ec = MonitorUtil::read_file(content, dir);

    REQUIRE(ec == std::make_error_code(std::errc::is_a_directory));
    std::error_code remove_ec;
    fs::remove_all(dir, remove_ec);
  }

  SECTION("returns ENOENT for missing file") {
    auto dir = make_temp_case_dir("read_file_missing");
    auto missing_file = dir / "missing.txt";

    std::string content;
    auto ec = MonitorUtil::read_file(content, missing_file);

    REQUIRE(ec);
    REQUIRE(ec.value() == ENOENT);
    std::error_code remove_ec;
    fs::remove_all(dir, remove_ec);
  }
}

TEST_CASE("MonitorUtil::string2integer", "[monitor_util]") {
  SECTION("parses decimal") {
    uint32_t value = 0;
    auto ec = MonitorUtil::string2integer("123", value);
    REQUIRE(ec == std::errc());
    REQUIRE(value == 123);
  }

  SECTION("parses hex") {
    uint32_t value = 0;
    auto ec = MonitorUtil::string2integer("0x10", value);
    REQUIRE(ec == std::errc());
    REQUIRE(value == 16);
  }

  SECTION("parses binary") {
    uint32_t value = 0;
    auto ec = MonitorUtil::string2integer("0b1010", value);
    REQUIRE(ec == std::errc());
    REQUIRE(value == 10);
  }

  SECTION("parses octal") {
    uint32_t value = 0;
    auto ec = MonitorUtil::string2integer("077", value);
    REQUIRE(ec == std::errc());
    REQUIRE(value == 63);
  }

  SECTION("returns invalid_argument on empty input") {
    uint32_t value = 99;
    auto ec = MonitorUtil::string2integer("", value);
    REQUIRE(ec == std::errc::invalid_argument);
    REQUIRE(value == 99);
  }

  SECTION("returns invalid_argument on invalid number") {
    uint32_t value = 0;
    auto ec = MonitorUtil::string2integer("not_a_number", value);
    REQUIRE(ec == std::errc::invalid_argument);
  }
}

TEST_CASE("MonitorUtil::add_watchpoint_monitor_attr", "[monitor_util]") {
  SECTION("fills attr fields from fake sysfs device files") {
    auto root = make_temp_case_dir("watchpoint_ok");
    auto device_path = root / "sys" / "bus" / "event_source" / "devices" / "fake_device";

    write_text_file(device_path / "type", "7");
    // Keep event value non-numeric in this unit test.
    // Numeric tokens (e.g. event=0x76) make PerfEventAttr::add_event()
    // read hardcoded /sys/.../format files, which turns this into a host-
    // dependent integration test.
    write_text_file(device_path / "events" / "watchpoint_up", "event=x23\n");

    write_text_file(device_path / "format" / "bynodeid", "config:0-0");
    write_text_file(device_path / "format" / "nodeid", "config:1-8");
    write_text_file(device_path / "format" / "wp_dev_sel", "config:9-9");
    write_text_file(device_path / "format" / "wp_chn_sel", "config:10-11");
    write_text_file(device_path / "format" / "wp_val", "config1:0-15");
    write_text_file(device_path / "format" / "wp_mask", "config2:0-15");

    PerfEventAttr attr;
    auto ec = MonitorUtil::add_watchpoint_monitor_attr(device_path, 0x1f, "watchpoint_up", attr);

    REQUIRE_FALSE(ec);
    auto* raw = attr.get();
    REQUIRE(raw->type == 7);
    REQUIRE(raw->disabled == 1);
    REQUIRE((raw->read_format & PERF_FORMAT_TOTAL_TIME_ENABLED) != 0);
    REQUIRE((raw->read_format & PERF_FORMAT_TOTAL_TIME_RUNNING) != 0);
    REQUIRE(raw->config != 0);

    std::error_code remove_ec;
    fs::remove_all(root, remove_ec);
  }

  SECTION("returns error when required file is missing") {
    auto root = make_temp_case_dir("watchpoint_missing");
    auto device_path = root / "sys" / "bus" / "event_source" / "devices" / "fake_device";

    write_text_file(device_path / "events" / "watchpoint_up", "event=x76\n");

    PerfEventAttr attr;
    auto ec = MonitorUtil::add_watchpoint_monitor_attr(device_path, 0x10, "watchpoint_up", attr);

    REQUIRE(ec);

    std::error_code remove_ec;
    fs::remove_all(root, remove_ec);
  }
}
