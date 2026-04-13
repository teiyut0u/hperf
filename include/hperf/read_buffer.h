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
  // Verify that the struct layout is all uint64_t fields with no padding
  static_assert(sizeof(Header) == 3 * sizeof(uint64_t), "Header layout mismatch");
  static_assert(sizeof(Entry) == 2 * sizeof(uint64_t), "Entry layout mismatch");

  explicit GroupReadBuffer(size_t event_num)
      : event_num_(event_num), buf_((header_size() + event_num * entry_size()) / sizeof(uint64_t)) {}

  /** @brief Return the number of events this buffer was sized for. */
  size_t event_num() const { return event_num_; }

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
  size_t size() const { return buf_.size() * sizeof(uint64_t); }

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
    const Entry* base = reinterpret_cast<const Entry*>(buf_.data() + header_size() / sizeof(uint64_t));
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
  size_t event_num_;
  std::vector<uint64_t> buf_;

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
  static_assert(sizeof(Entry) == 4 * sizeof(uint64_t), "SingleReadBuffer::Entry layout mismatch");

  explicit SingleReadBuffer()
      : buf_(sizeof(Entry) / sizeof(uint64_t)) {}

  /**
   * @brief Get the pointer to the single reading buffer
   *
   * @return void*
   */
  void* data() { return buf_.data(); }

  /**
   * @brief Get the size in bytes of the single reading buffer
   *
   * @return size_t
   */
  size_t size() const { return buf_.size() * sizeof(uint64_t); }

  uint64_t value() const { return entry()->value; }

  uint64_t time_enabled() const { return entry()->time_enabled; }

  uint64_t time_running() const { return entry()->time_running; }

  uint64_t id() const { return entry()->id; }

 private:
  /**
   * @brief The buffer to store the data read from perf_event_open fd
   *
   */
  std::vector<uint64_t> buf_;

  const Entry* entry() const {
    return reinterpret_cast<const Entry*>(buf_.data());
  }
};