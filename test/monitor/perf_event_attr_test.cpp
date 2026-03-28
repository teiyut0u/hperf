#include "hperf/monitor/perf_event_attr.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

void write_text_file(const fs::path& path, const std::string& content) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path);
  REQUIRE(out.is_open());
  out << content;
}

fs::path make_temp_case_dir(const std::string& case_name) {
  auto dir = fs::temp_directory_path() / ("hperf_perf_event_attr_test_" + case_name);
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  REQUIRE_FALSE(ec);
  return dir;
}

}  // namespace

TEST_CASE("PerfEventAttr::set_type", "[perf_event_attr]") {
  SECTION("reads numeric type from file") {
    auto dir = make_temp_case_dir("set_type_ok");
    auto type_file = dir / "type";
    write_text_file(type_file, "9");

    PerfEventAttr attr;
    auto ec = attr.set_type(type_file);

    REQUIRE_FALSE(ec);
    REQUIRE(attr.get()->type == 9);

    std::error_code remove_ec;
    fs::remove_all(dir, remove_ec);
  }

  SECTION("returns error for missing file") {
    auto dir = make_temp_case_dir("set_type_missing");

    PerfEventAttr attr;
    auto ec = attr.set_type(dir / "type");

    REQUIRE(ec);
    REQUIRE(ec.value() == ENOENT);

    std::error_code remove_ec;
    fs::remove_all(dir, remove_ec);
  }
}

TEST_CASE("PerfEventAttr::add_field", "[perf_event_attr]") {
  SECTION("writes bits into config") {
    auto dir = make_temp_case_dir("add_field_config");
    auto field_file = dir / "field";
    write_text_file(field_file, "config:4-7");

    PerfEventAttr attr;
    auto ec = attr.add_field(field_file, 0b1010);

    REQUIRE_FALSE(ec);
    REQUIRE(((attr.get()->config >> 4) & 0xF) == 0b1010);

    std::error_code remove_ec;
    fs::remove_all(dir, remove_ec);
  }

  SECTION("writes bits into config1") {
    auto dir = make_temp_case_dir("add_field_config1");
    auto field_file = dir / "field";
    write_text_file(field_file, "config1:0-3");

    PerfEventAttr attr;
    auto ec = attr.add_field(field_file, 0b1101);

    REQUIRE_FALSE(ec);
    REQUIRE((attr.get()->config1 & 0xF) == 0b1101);

    std::error_code remove_ec;
    fs::remove_all(dir, remove_ec);
  }

  SECTION("returns error for missing field file") {
    auto dir = make_temp_case_dir("add_field_missing");

    PerfEventAttr attr;
    auto ec = attr.add_field(dir / "no_such_field", 1);

    REQUIRE(ec);
    REQUIRE(ec.value() == ENOENT);

    std::error_code remove_ec;
    fs::remove_all(dir, remove_ec);
  }
}

TEST_CASE("PerfEventAttr::add_event", "[perf_event_attr]") {
  SECTION("rejects path without devices component") {
    PerfEventAttr attr;
    auto ec = attr.add_event("/tmp/not_devices/events/event0");

    REQUIRE(ec == std::make_error_code(std::errc::invalid_argument));
  }

  SECTION("returns error when event file is missing") {
    PerfEventAttr attr;
    auto ec = attr.add_event("/tmp/devices/fake_device/events/no_event");

    REQUIRE(ec);
    REQUIRE(ec.value() == ENOENT);
  }
}

TEST_CASE("PerfEventAttr::parse_attr_from", "[perf_event_attr]") {
  SECTION("rejects invalid format without slash") {
    PerfEventAttr attr;
    auto ec = attr.parse_attr_from("arm_cmn_watchpoint_up");

    REQUIRE(ec == std::make_error_code(std::errc::invalid_argument));
  }

  SECTION("rejects invalid format without trailing slash") {
    PerfEventAttr attr;
    auto ec = attr.parse_attr_from("arm_cmn/watchpoint_up");

    REQUIRE(ec == std::make_error_code(std::errc::invalid_argument));
  }

  SECTION("returns error for valid format when device files are unavailable") {
    PerfEventAttr attr;
    auto ec = attr.parse_attr_from("definitely_not_exist_device/watchpoint_up/");

    REQUIRE(ec);
  }
}
