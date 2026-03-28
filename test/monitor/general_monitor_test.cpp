#include "hperf/monitor/general_monitor.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <memory>
#include <vector>

#include "mocks/mock_event_controller.hpp"

TEST_CASE("GeneralMonitor::set_monitor", "[general_monitor]") {
  GeneralMonitor monitor;

  SECTION("set_monitor with single controller") {
    auto controller = std::make_unique<MockEventController>();
    controller->set_values({42});

    std::vector<std::unique_ptr<EventControllerInterface>> controllers;
    controllers.push_back(std::move(controller));

    auto result = monitor.set_controller(std::move(controllers));
    REQUIRE(result == std::errc());
    REQUIRE(monitor.get_size() == std::vector<size_t>{1});
  }

  SECTION("set_monitor with multiple controllers") {
    auto controller1 = std::make_unique<MockEventController>();
    auto controller2 = std::make_unique<MockEventController>();
    controller1->set_size(2);
    controller2->set_size(3);

    std::vector<std::unique_ptr<EventControllerInterface>> controllers;
    controllers.push_back(std::move(controller1));
    controllers.push_back(std::move(controller2));

    auto result = monitor.set_controller(std::move(controllers));
    REQUIRE(result == std::errc());
    REQUIRE(monitor.get_size() == std::vector<size_t>{2, 3});
  }

  SECTION("set_monitor clears previous state") {
    // First set
    {
      auto controller = std::make_unique<MockEventController>();
      std::vector<std::unique_ptr<EventControllerInterface>> controllers;
      controllers.push_back(std::move(controller));
      monitor.set_controller(std::move(controllers));
      REQUIRE(monitor.get_size().size() == 1);
    }

    // Second set should clear first
    {
      auto controller1 = std::make_unique<MockEventController>();
      auto controller2 = std::make_unique<MockEventController>();
      std::vector<std::unique_ptr<EventControllerInterface>> controllers;
      controllers.push_back(std::move(controller1));
      controllers.push_back(std::move(controller2));
      monitor.set_controller(std::move(controllers));
      REQUIRE(monitor.get_size().size() == 2);
    }
  }

  SECTION("set_monitor rejects invalid read_format") {
    auto controller = std::make_unique<MockEventController>();
    controller->set_read_format(0);  // Missing required flags

    std::vector<std::unique_ptr<EventControllerInterface>> controllers;
    controllers.push_back(std::move(controller));

    auto result = monitor.set_controller(std::move(controllers));
    REQUIRE(result == std::errc::invalid_argument);
  }
}

TEST_CASE("GeneralMonitor::add_monitor", "[general_monitor]") {
  GeneralMonitor monitor;

  SECTION("add_monitor with vector") {
    auto controller = std::make_unique<MockEventController>();
    std::vector<std::unique_ptr<EventControllerInterface>> controllers;
    controllers.push_back(std::move(controller));

    auto result = monitor.add_controller(std::move(controllers));
    REQUIRE(result == std::errc());
    REQUIRE(monitor.get_size().size() == 1);
  }

  SECTION("add_monitor accumulates controllers") {
    // Add first controller
    {
      auto controller = std::make_unique<MockEventController>();
      std::vector<std::unique_ptr<EventControllerInterface>> controllers;
      controllers.push_back(std::move(controller));
      auto result = monitor.add_controller(std::move(controllers));
      REQUIRE(result == std::errc());
    }

    // Add second controller
    {
      auto controller = std::make_unique<MockEventController>();
      std::vector<std::unique_ptr<EventControllerInterface>> controllers;
      controllers.push_back(std::move(controller));
      auto result = monitor.add_controller(std::move(controllers));
      REQUIRE(result == std::errc());
    }

    REQUIRE(monitor.get_size().size() == 2);
  }

  SECTION("add_monitor with single controller") {
    auto controller = std::make_unique<MockEventController>();
    auto result = monitor.add_controller(std::move(controller));
    REQUIRE(result == std::errc());
    REQUIRE(monitor.get_size().size() == 1);
  }

  SECTION("add_monitor rejects invalid read_format") {
    auto controller = std::make_unique<MockEventController>();
    controller->set_read_format(0);

    auto result = monitor.add_controller(std::move(controller));
    REQUIRE(result == std::errc::invalid_argument);
  }
}

TEST_CASE("GeneralMonitor::clear", "[general_monitor]") {
  GeneralMonitor monitor;

  auto controller = std::make_unique<MockEventController>();
  std::vector<std::unique_ptr<EventControllerInterface>> controllers;
  controllers.push_back(std::move(controller));
  monitor.set_controller(std::move(controllers));

  REQUIRE(monitor.get_size().size() == 1);
  monitor.clear();
  REQUIRE(monitor.get_size().size() == 0);
}

TEST_CASE("GeneralMonitor::start and stop", "[general_monitor]") {
  GeneralMonitor monitor;

  SECTION("start calls reset and enable on all controllers") {
    auto controller = std::make_unique<MockEventController>();
    auto controller_ptr = controller.get();

    std::vector<std::unique_ptr<EventControllerInterface>> controllers;
    controllers.push_back(std::move(controller));
    monitor.set_controller(std::move(controllers));

    auto result = monitor.start();
    REQUIRE(result.value() == 0);
    REQUIRE(controller_ptr->get_control_call_count() == 2);  // reset + enable
  }

  SECTION("stop calls disable on all controllers") {
    auto controller = std::make_unique<MockEventController>();
    auto controller_ptr = controller.get();

    std::vector<std::unique_ptr<EventControllerInterface>> controllers;
    controllers.push_back(std::move(controller));
    monitor.set_controller(std::move(controllers));

    monitor.start();
    controller_ptr->reset_call_counts();

    auto result = monitor.stop();
    REQUIRE(result.value() == 0);
    REQUIRE(controller_ptr->get_control_call_count() == 1);  // disable
  }

  SECTION("start returns error if reset fails") {
    auto controller = std::make_unique<MockEventController>();
    controller->set_control_error(std::make_error_code(std::errc::io_error));

    std::vector<std::unique_ptr<EventControllerInterface>> controllers;
    controllers.push_back(std::move(controller));
    monitor.set_controller(std::move(controllers));

    auto result = monitor.start();
    REQUIRE(result == std::make_error_code(std::errc::io_error));
  }

  SECTION("stop returns error if disable fails") {
    auto controller = std::make_unique<MockEventController>();
    auto controller_ptr = controller.get();

    std::vector<std::unique_ptr<EventControllerInterface>> controllers;
    controllers.push_back(std::move(controller));
    monitor.set_controller(std::move(controllers));

    monitor.start();

    controller_ptr->set_control_error(std::make_error_code(std::errc::permission_denied));
    auto result = monitor.stop();
    REQUIRE(result == std::make_error_code(std::errc::permission_denied));
  }
}

TEST_CASE("GeneralMonitor::reset", "[general_monitor]") {
  GeneralMonitor monitor;

  auto controller = std::make_unique<MockEventController>();
  controller->set_size(2);
  controller->set_values({100, 200});
  controller->set_time_enabled(5000);
  controller->set_time_running(4000);

  std::vector<std::unique_ptr<EventControllerInterface>> controllers;
  controllers.push_back(std::move(controller));
  monitor.set_controller(std::move(controllers));

  // Before reset, internal state should have non-zero values
  std::vector<uint64_t> scaled_counts;
  monitor.get_scaled_count(scaled_counts);

  monitor.reset();

  // After reset, baseline is cleared to zero. With unchanged controller values,
  // the next read is computed from zero baseline again, so it matches the first read.
  scaled_counts.clear();
  monitor.get_scaled_count(scaled_counts);

  // scaled = round(5000 / 4000 * value)
  REQUIRE(scaled_counts[0] == 125);
  REQUIRE(scaled_counts[1] == 250);
}

TEST_CASE("GeneralMonitor::get_size", "[general_monitor]") {
  GeneralMonitor monitor;

  SECTION("empty monitor") {
    REQUIRE(monitor.get_size().size() == 0);
  }

  SECTION("returns size from each controller") {
    auto controller1 = std::make_unique<MockEventController>();
    auto controller2 = std::make_unique<MockEventController>();
    controller1->set_size(3);
    controller2->set_size(5);

    std::vector<std::unique_ptr<EventControllerInterface>> controllers;
    controllers.push_back(std::move(controller1));
    controllers.push_back(std::move(controller2));
    monitor.set_controller(std::move(controllers));

    auto sizes = monitor.get_size();
    REQUIRE(sizes.size() == 2);
    REQUIRE(sizes[0] == 3);
    REQUIRE(sizes[1] == 5);
  }
}

TEST_CASE("GeneralMonitor::get_scaled_count", "[general_monitor]") {
  GeneralMonitor monitor;

  SECTION("single reading") {
    auto controller = std::make_unique<MockEventController>();
    auto controller_ptr = controller.get();
    controller->set_values({100});
    controller->set_time_enabled(1000);
    controller->set_time_running(1000);

    std::vector<std::unique_ptr<EventControllerInterface>> controllers;
    controllers.push_back(std::move(controller));
    monitor.set_controller(std::move(controllers));

    std::vector<uint64_t> scaled_counts;
    auto result = monitor.get_scaled_count(scaled_counts);

    REQUIRE(result.value() == 0);
    REQUIRE(controller_ptr->get_read_call_count() == 1);
    REQUIRE(scaled_counts.size() == 1);
    // First reading: delta_value = 100 - 0 = 100
    // scaled = round(1000 / 1000 * 100) = 100
    REQUIRE(scaled_counts[0] == 100);
  }

  SECTION("multiple readings calculate delta correctly") {
    auto controller = std::make_unique<MockEventController>();
    auto controller_ptr = controller.get();
    controller->set_values({100});
    controller->set_time_enabled(1000);
    controller->set_time_running(1000);

    std::vector<std::unique_ptr<EventControllerInterface>> controllers;
    controllers.push_back(std::move(controller));
    monitor.set_controller(std::move(controllers));

    // First reading
    std::vector<uint64_t> scaled_counts;
    monitor.get_scaled_count(scaled_counts);
    REQUIRE(scaled_counts[0] == 100);

    // Second reading with updated values
    controller_ptr->set_values({150});
    controller_ptr->set_time_enabled(2000);
    controller_ptr->set_time_running(2000);

    scaled_counts.clear();
    monitor.get_scaled_count(scaled_counts);
    // delta_value = 150 - 100 = 50
    // delta_time_enabled = 2000 - 1000 = 1000
    // scaled = round(1000 / 1000 * 50) = 50
    REQUIRE(scaled_counts[0] == 50);
  }

  SECTION("scaled count with time running less than enabled") {
    auto controller = std::make_unique<MockEventController>();
    controller->set_values({100});
    controller->set_time_enabled(1000);
    controller->set_time_running(500);

    std::vector<std::unique_ptr<EventControllerInterface>> controllers;
    controllers.push_back(std::move(controller));
    monitor.set_controller(std::move(controllers));

    std::vector<uint64_t> scaled_counts;
    monitor.get_scaled_count(scaled_counts);

    // scaled = round(1000 / 500 * 100) = round(200) = 200
    REQUIRE(scaled_counts[0] == 200);
  }

  SECTION("zero time_enabled results in zero count") {
    auto controller = std::make_unique<MockEventController>();
    controller->set_values({100});
    controller->set_time_enabled(0);
    controller->set_time_running(1000);

    std::vector<std::unique_ptr<EventControllerInterface>> controllers;
    controllers.push_back(std::move(controller));
    monitor.set_controller(std::move(controllers));

    std::vector<uint64_t> scaled_counts;
    monitor.get_scaled_count(scaled_counts);

    // scaled = 0 (due to delta_time_enabled == 0)
    REQUIRE(scaled_counts[0] == 0);
  }

  SECTION("get_scaled_count returns error if read fails") {
    auto controller = std::make_unique<MockEventController>();
    controller->set_read_error(std::make_error_code(std::errc::io_error));

    std::vector<std::unique_ptr<EventControllerInterface>> controllers;
    controllers.push_back(std::move(controller));
    monitor.set_controller(std::move(controllers));

    std::vector<uint64_t> scaled_counts;
    auto result = monitor.get_scaled_count(scaled_counts);
    REQUIRE(result == std::make_error_code(std::errc::io_error));
  }

  SECTION("multiple controllers") {
    auto controller1 = std::make_unique<MockEventController>();
    auto controller2 = std::make_unique<MockEventController>();
    controller1->set_values({100});
    controller1->set_time_enabled(2000);
    controller1->set_time_running(2000);

    controller2->set_size(2);
    controller2->set_values({50, 75});
    controller2->set_time_enabled(1000);
    controller2->set_time_running(1000);

    std::vector<std::unique_ptr<EventControllerInterface>> controllers;
    controllers.push_back(std::move(controller1));
    controllers.push_back(std::move(controller2));
    monitor.set_controller(std::move(controllers));

    std::vector<uint64_t> scaled_counts;
    monitor.get_scaled_count(scaled_counts);

    REQUIRE(scaled_counts.size() == 3);
    REQUIRE(scaled_counts[0] == 100);  // controller1
    REQUIRE(scaled_counts[1] == 50);   // controller2 first value
    REQUIRE(scaled_counts[2] == 75);   // controller2 second value
  }
}
