#include "hperf/counter_detector.h"

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <ostream>

#include "hperf/read_buffer.h"

CounterDetector::CounterDetector()
    : detected_(false),
      fds_(),
      cpu_num_(sysconf(_SC_NPROCESSORS_ONLN)),
      detected_general_counter_nums_(cpu_num_, -1) {}

CounterDetector::~CounterDetector() {
  close_all_events();
}

void CounterDetector::detect() {
  if (load_detected_result()) {
    return;
  }

  for (uint64_t cpu_id = 0; cpu_id < cpu_num_; ++cpu_id) {  // for each CPU
    // Stress test: guadually increase the number of events (up to 32), and see when multiplexing is triggered:
    for (uint64_t event_num = 1; event_num < event_list.size(); ++event_num) {
      if (!test(cpu_id, event_num)) {  // if multiplexing is triggered
        detected_general_counter_nums_[cpu_id] = event_num - 1;
        break;  // stop stress test on this CPU
      }
    }
    close_all_events();
    fds_.clear();
  }
  detected_ = true;
  save_detected_result();
}

bool CounterDetector::test(uint64_t cpu_id, uint64_t event_num) {
  // Ensure fds_ has enough capacity for event_num events
  while (fds_.size() < event_num) {
    struct perf_event_attr pe;
    configure_event(&pe, event_list[fds_.size()].second);
    int fd = perf_event_open(&pe, cpu_id);
    if (fd == -1) {
      std::cerr << "Failed to create event " << event_list[fds_.size()].first << " on CPU " << cpu_id << std::endl;
      return false;
    }
    fds_.push_back(fd);
  }

  // Enable all events and check for multiplexing
  if (!enable_all_events()) {
    return false;
  }

  usleep(100000);  // Sleep for 100ms

  if (!disable_all_events()) {
    return false;
  }

  // Read and check if multiplexing occurred
  SingleReadBuffer buffer;
  for (int fd : fds_) {
    ssize_t bytes_read = read(fd, buffer.data(), buffer.size());
    if (bytes_read == -1) {
      std::cerr << "Failed to read data for event: " << strerror(errno) << std::endl;
      return false;
    } else if (static_cast<size_t>(bytes_read) != buffer.size()) {
      std::cerr << "Warning: Read " << bytes_read << " bytes, expected " << buffer.size() << std::endl;
    } else {
      if (buffer.time_enabled() != buffer.time_running()) {  // Multiplexing detected
        return false;
      }
    }
  }

  // For all events, time_enabled == time_running, return true
  return true;
}

bool CounterDetector::enable_all_events() {
  for (const int fd : fds_) {
    if (ioctl(fd, PERF_EVENT_IOC_ENABLE, 0) == -1) {
      std::cerr << "Failed to enable event: " << strerror(errno) << std::endl;
      return false;
    }
  }
  return true;
}

bool CounterDetector::disable_all_events() {
  for (const int fd : fds_) {
    if (ioctl(fd, PERF_EVENT_IOC_DISABLE, 0) == -1) {
      std::cerr << "Failed to disable event: " << strerror(errno) << std::endl;
      return false;
    }
  }
  return true;
}

void CounterDetector::close_all_events() {
  for (int fd : fds_) {
    if (fd != -1) {
      close(fd);
    }
  }
}

int CounterDetector::get_detected_general_counter_num(uint64_t cpu_id) const {
  if (!detected_) {
    std::cerr << "The number of avaliable programmable counters is undetected" << std::endl;
    return -1;
  }
  if (cpu_id >= cpu_num_) {
    std::cerr << "CPU ID is out-of-bound" << std::endl;
    return -1;
  }
  return detected_general_counter_nums_[cpu_id];
}

int CounterDetector::get_detected_general_counter_num() const {
  if (!detected_) {
    std::cerr << "The number of avaliable programmable counters is undetected" << std::endl;
    return -1;
  }
  return *std::min_element(detected_general_counter_nums_.begin(),
                           detected_general_counter_nums_.end());
}

void CounterDetector::print_result() const {
  if (!detected_) {
    std::cerr << "The number of avaliable programmable counters is undetected" << std::endl;
    return;
  }
  for (uint64_t cpu_id = 0; cpu_id < cpu_num_; ++cpu_id) {
    if (detected_general_counter_nums_[cpu_id] > 0) {
      std::cout << detected_general_counter_nums_[cpu_id] << " available programmable counters on CPU " << cpu_id << std::endl;
    } else {
      std::cout << "Undetected on CPU " << cpu_id << std::endl;
    }
  }
}

void CounterDetector::configure_event(struct perf_event_attr *pe,
                                      uint64_t encoding) {
  memset(pe, 0, sizeof(struct perf_event_attr));

  pe->type = PERF_TYPE_RAW;
  pe->size = sizeof(struct perf_event_attr);
  pe->config = encoding;
  pe->read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_ID;
  pe->disabled = 1;
}

int CounterDetector::perf_event_open(struct perf_event_attr *pe,
                                     int cpu_id) {
  int fd = syscall(__NR_perf_event_open, pe, -1, cpu_id, -1, 0);
  if (fd == -1) {
    std::cerr << "perf_event_open failed for event 0x" << std::hex << pe->config
              << std::dec << " on CPU " << cpu_id
              << " (-1 represents any CPU): " << strerror(errno) << "\n";
  }
  return fd;
}

void CounterDetector::save_detected_result() const {
  std::ofstream outfile("/tmp/.hperf");
  if (!outfile.is_open()) {
    std::cerr << "Failed to create /tmp/.hperf" << std::endl;
    return;
  }

  for (const auto& counter_num : detected_general_counter_nums_) {
    outfile << counter_num << std::endl;
  }

  outfile.close();
}

bool CounterDetector::load_detected_result() {
  std::ifstream infile("/tmp/.hperf");
  if (!infile.is_open()) {
    return false;
  }

  detected_general_counter_nums_.clear();
  int counter_num;
  while (infile >> counter_num) {
    detected_general_counter_nums_.push_back(counter_num);
  }

  infile.close();
  
  if (detected_general_counter_nums_.size() == cpu_num_) {
    detected_ = true;
    return true;
  }
  
  detected_general_counter_nums_.clear();
  detected_general_counter_nums_.resize(cpu_num_, -1);
  return false;
}

