#include "hperf/monitor/event_controller/single_event_controller.hpp"

#include <sys/syscall.h>
#include <unistd.h>

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cerrno>
#include <cstdint>
#include <vector>

#include "mocks/perf_mocks.hpp"

namespace {

struct PerfMockGuard {
  PerfMockGuard() { PerfMocks::instance().reset(); }
  ~PerfMockGuard() { PerfMocks::instance().reset(); }
};

}  // namespace

TEST_CASE("SingleEventController::open_event", "[single_event_controller]") {
  PerfMockGuard guard;

  SECTION("returns errno when perf_event_open fails") {
    SingleEventController controller;

    perf_event_attr attr{};
    attr.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

    PerfMocks::instance().set_syscall_handler(
        [](void*, pid_t, int, int, unsigned long) {
          errno = EACCES;
          return -1;
        });

    auto ec = controller.open_event(&attr, 0, 0, 0);

    REQUIRE(ec);
    REQUIRE(ec.value() == EACCES);
    REQUIRE(attr.read_format ==
            (PERF_FORMAT_GROUP | PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING));
  }

  SECTION("strips PERF_FORMAT_GROUP for internal read format and restores caller attr") {
    SingleEventController controller;

    perf_event_attr attr{};
    attr.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_TOTAL_TIME_ENABLED |
                       PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_ID |
                       PERF_FORMAT_LOST;
    const auto original_read_format = attr.read_format;

    PerfMocks::instance().set_syscall_handler(
        [](void* attr_ptr, pid_t pid, int cpu, int group_fd, unsigned long flags) {
          REQUIRE(pid == 123);
          REQUIRE(cpu == 4);
          REQUIRE(group_fd == -1);
          REQUIRE(flags == 8);

          auto* perf_attr = reinterpret_cast<perf_event_attr*>(attr_ptr);
          REQUIRE((perf_attr->read_format & PERF_FORMAT_GROUP) == 0);
          return 10;
        });

    auto ec = controller.open_event(&attr, 123, 4, 8);

    REQUIRE_FALSE(ec);
    REQUIRE(attr.read_format == original_read_format);
    REQUIRE((controller.get_read_format() & PERF_FORMAT_GROUP) == 0);
    REQUIRE((controller.get_read_format() & PERF_FORMAT_TOTAL_TIME_ENABLED) != 0);
    REQUIRE((controller.get_read_format() & PERF_FORMAT_TOTAL_TIME_RUNNING) != 0);
    REQUIRE((controller.get_read_format() & PERF_FORMAT_ID) != 0);
    REQUIRE((controller.get_read_format() & PERF_FORMAT_LOST) != 0);
  }
}

TEST_CASE("SingleEventController::control", "[single_event_controller]") {
  PerfMockGuard guard;

  SECTION("returns bad_file_descriptor before open_event") {
    SingleEventController controller;
    auto ec = controller.control(PERF_EVENT_IOC_ENABLE, nullptr);

    REQUIRE(ec == std::make_error_code(std::errc::bad_file_descriptor));
  }

  SECTION("returns success when ioctl succeeds") {
    SingleEventController controller;

    perf_event_attr attr{};
    attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

    PerfMocks::instance().set_syscall_handler(
        [](void*, pid_t, int, int, unsigned long) { return 20; });
    PerfMocks::instance().set_ioctl_handler(
        [](int fd, unsigned long request, void* arg) {
          REQUIRE(fd == 20);
          REQUIRE(request == PERF_EVENT_IOC_ENABLE);
          REQUIRE(arg == nullptr);
          return 0;
        });

    auto open_ec = controller.open_event(&attr, 0, 0, 0);
    REQUIRE_FALSE(open_ec);

    auto ec = controller.control(PERF_EVENT_IOC_ENABLE, nullptr);
    REQUIRE_FALSE(ec);
  }

  SECTION("returns errno when ioctl fails") {
    SingleEventController controller;

    perf_event_attr attr{};
    attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

    PerfMocks::instance().set_syscall_handler(
        [](void*, pid_t, int, int, unsigned long) { return 21; });
    PerfMocks::instance().set_ioctl_handler(
        [](int fd, unsigned long request, void*) {
          REQUIRE(fd == 21);
          REQUIRE(request == PERF_EVENT_IOC_DISABLE);
          errno = EPERM;
          return -1;
        });

    auto open_ec = controller.open_event(&attr, 0, 0, 0);
    REQUIRE_FALSE(open_ec);

    auto ec = controller.control(PERF_EVENT_IOC_DISABLE, nullptr);
    REQUIRE(ec);
    REQUIRE(ec.value() == EPERM);
  }
}

TEST_CASE("SingleEventController move semantics", "[single_event_controller]") {
  PerfMockGuard guard;

  SECTION("move constructor transfers opened fd ownership") {
    SingleEventController source;

    perf_event_attr attr{};
    attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;

    PerfMocks::instance().set_syscall_handler(
        [](void*, pid_t, int, int, unsigned long) { return 30; });
    PerfMocks::instance().set_ioctl_handler(
        [](int fd, unsigned long request, void*) {
          REQUIRE(fd == 30);
          REQUIRE(request == PERF_EVENT_IOC_ENABLE);
          return 0;
        });

    auto open_ec = source.open_event(&attr, 0, 0, 0);
    REQUIRE_FALSE(open_ec);

    SingleEventController moved(std::move(source));

    auto source_ec = source.control(PERF_EVENT_IOC_ENABLE, nullptr);
    REQUIRE(source_ec == std::make_error_code(std::errc::bad_file_descriptor));

    auto moved_ec = moved.control(PERF_EVENT_IOC_ENABLE, nullptr);
    REQUIRE_FALSE(moved_ec);
  }
}

TEST_CASE("SingleEventController::read and value accessors", "[single_event_controller]") {
  PerfMockGuard guard;

  SECTION("read returns errno for invalid fd") {
    SingleEventController controller;
    auto ec = controller.read();

    REQUIRE(ec);
    REQUIRE(ec.value() == EBADF);
  }

  SECTION("read fills buffer and accessors return parsed values") {
    SingleEventController controller;

    int fds[2];
    REQUIRE(::pipe(fds) == 0);
    const int read_fd = fds[0];
    const int write_fd = fds[1];

    perf_event_attr attr{};
    attr.read_format = PERF_FORMAT_TOTAL_TIME_ENABLED |
                       PERF_FORMAT_TOTAL_TIME_RUNNING |
                       PERF_FORMAT_ID |
                       PERF_FORMAT_LOST;

    PerfMocks::instance().set_syscall_handler(
        [read_fd](void*, pid_t, int cpu, int group_fd, unsigned long) {
          REQUIRE(group_fd == -1);
          return read_fd;
        });

    auto open_ec = controller.open_event(&attr, 0, 0, 0);
    REQUIRE_FALSE(open_ec);

    const std::array<uint64_t, 5> raw_data{
        111,   // value
        2222,  // time_enabled
        2000,  // time_running
        55,    // id
        3      // lost
    };

    const auto* bytes = reinterpret_cast<const char*>(raw_data.data());
    const auto total_bytes = static_cast<size_t>(raw_data.size() * sizeof(uint64_t));
    ssize_t written = 0;
    while (static_cast<size_t>(written) < total_bytes) {
      auto n = ::write(write_fd, bytes + written, total_bytes - static_cast<size_t>(written));
      REQUIRE(n > 0);
      written += n;
    }
    REQUIRE(::close(write_fd) == 0);

    auto read_ec = controller.read();
    REQUIRE_FALSE(read_ec);

    REQUIRE(controller.value() == 111);
    REQUIRE(controller.all_value() == std::vector<uint64_t>{111});

    REQUIRE(controller.time_enabled().has_value());
    REQUIRE(controller.time_enabled().value() == 2222);

    REQUIRE(controller.time_running().has_value());
    REQUIRE(controller.time_running().value() == 2000);

    REQUIRE(controller.id().has_value());
    REQUIRE(controller.id().value() == 55);

    REQUIRE(controller.all_id().has_value());
    REQUIRE(controller.all_id().value() == std::vector<uint64_t>{55});

    REQUIRE(controller.lost().has_value());
    REQUIRE(controller.lost().value() == 3);

    REQUIRE(controller.all_lost().has_value());
    REQUIRE(controller.all_lost().value() == std::vector<uint64_t>{3});
  }

  SECTION("optional fields are empty when read_format does not include them") {
    SingleEventController controller;

    int fds[2];
    REQUIRE(::pipe(fds) == 0);
    const int read_fd = fds[0];
    const int write_fd = fds[1];

    perf_event_attr attr{};
    attr.read_format = 0;

    PerfMocks::instance().set_syscall_handler(
        [read_fd](void*, pid_t, int, int, unsigned long) { return read_fd; });

    auto open_ec = controller.open_event(&attr, 0, 0, 0);
    REQUIRE_FALSE(open_ec);

    uint64_t value = 999;
    auto n = ::write(write_fd, &value, sizeof(value));
    REQUIRE(n == static_cast<ssize_t>(sizeof(value)));
    REQUIRE(::close(write_fd) == 0);

    auto read_ec = controller.read();
    REQUIRE_FALSE(read_ec);

    REQUIRE(controller.value() == 999);
    REQUIRE_FALSE(controller.time_enabled().has_value());
    REQUIRE_FALSE(controller.time_running().has_value());
    REQUIRE_FALSE(controller.id().has_value());
    REQUIRE_FALSE(controller.all_id().has_value());
    REQUIRE_FALSE(controller.lost().has_value());
    REQUIRE_FALSE(controller.all_lost().has_value());
  }
}
