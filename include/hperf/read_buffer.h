#pragma once

#include <sys/types.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

/**
 * @brief The buffer for reading event counts in group.
 * Multiple event count values can be read in a single read() syscall.
 *
 */
class GroupReadBuffer {
 public:
  /* See https://www.man7.org/linux/man-pages/man2/perf_event_open.2.html Reading results */

  struct Header {
    uint64_t nr;            // The number of events in group
    uint64_t time_enabled;  // Time enabled (PERF_FORMAT_TOTAL_TIME_ENABLED)
    uint64_t time_running;  // Time running (PERF_FORMAT_TOTAL_TIME_RUNNING)
  };

  struct Entry {
    uint64_t value;  // The event count
    uint64_t id;     // A 64-bit globally unique value for this event (PERF_FORMAT_ID)
  };

  /**
   * @brief Construct a new Group Read Buffer object
   *
   * @param event_num The number of events (the number of fixed events + the number of schedulable events in an event group)
   */
  explicit GroupReadBuffer(size_t event_num)
      : buf_(header_size() + event_num * entry_size()) {}

  /**
   * @brief Get the pointer to the group reading buffer
   *
   * @return void*
   */
  void* data() { return buf_.data(); }

  /**
   * @brief Get the size in byte of the group reading buffer
   *
   * @return size_t
   */
  size_t size() const { return buf_.size(); }

  uint64_t nr() const { return header()->nr; }

  uint64_t time_enabled() const { return header()->time_enabled; }

  uint64_t time_running() const { return header()->time_running; }

  /**
   * @brief Get the event count entry by the index.
   * If the index out-of-bounds, return null.
   *
   * @param idx The index, starting from 0 (fixed events + schedulable events)
   * @return std::optional<Entry>
   */
  std::optional<Entry> entry(size_t idx) const {
    if (idx >= nr()) return std::nullopt;
    const Entry* base = reinterpret_cast<const Entry*>(buf_.data() + header_size());
    return base[idx];
  }

 private:
  /**
   * @brief The buffer to store the data read from perf_event_open fd
   *
   * Memory layout: (for n events in a event group)
     Header + Entry 1 + Entry 2 + ... + Entry n
     ^
     |
     buf_.data()
   */
  std::vector<std::byte> buf_;

  static constexpr size_t header_size() { return sizeof(Header); }

  static constexpr size_t entry_size() { return sizeof(Entry); }

  const Header* header() const {
    return reinterpret_cast<const Header*>(buf_.data());
  }
};

class SingleReadBuffer {
 public:
  /* See https://www.man7.org/linux/man-pages/man2/perf_event_open.2.html Reading results */

  struct Entry {
    uint64_t value;         // The event count
    uint64_t time_enabled;  // Time enabled (PERF_FORMAT_TOTAL_TIME_ENABLED)
    uint64_t time_running;  // Time running (PERF_FORMAT_TOTAL_TIME_RUNNING)
    uint64_t id;            // A 64-bit globally unique value for this event (PERF_FORMAT_ID)
  };

  /**
   * @brief Construct a new Single Read Buffer object
   *
   */
  explicit SingleReadBuffer()
      : buf_(sizeof(Entry)) {}

  /**
   * @brief Get the pointer to the group reading buffer
   *
   * @return void*
   */
  void* data() { return buf_.data(); }

  /**
   * @brief Get the size in byte of the group reading buffer
   *
   * @return size_t
   */
  size_t size() const { return buf_.size(); }

  uint64_t value() const { return entry()->value; }

  uint64_t time_enabled() const { return entry()->time_enabled; }

  uint64_t time_running() const { return entry()->time_running; }

  uint64_t id() const { return entry()->id; }

 private:
  /**
   * @brief The buffer to store the data read from perf_event_open fd
   *
   */
  std::vector<std::byte> buf_;

  const Entry* entry() const {
    return reinterpret_cast<const Entry*>(buf_.data());
  }
};