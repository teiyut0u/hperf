#pragma once

#include <cstdint>
#include <utility>
#include <vector>
#include <string>

class CounterDetector {
 public:
  CounterDetector();

  ~CounterDetector();

  /**
   * @brief Detect the number of available progammable counters on each CPU.
   * Then the detected result can be obtained by `get_detected_general_counter_num()`
   * 
   */
  void detect();

  int get_detected_general_counter_num(uint64_t cpu_id) const;

  int get_detected_general_counter_num() const; 

  void print_result() const;

  /**
   * @brief Save detected counter numbers to /tmp/.hperf file
   */
  void save_detected_result() const;

  /**
   * @brief Load detected counter numbers from /tmp/.hperf file
   * @return true if file exists and load succeeded, false otherwise
   */
  bool load_detected_result();

 private:
  bool detected_;

  std::vector<int> fds_;

  uint64_t cpu_num_;

  std::vector<int> detected_general_counter_nums_;

  static void configure_event(struct perf_event_attr *pe,
                              uint64_t encoding);

  static int perf_event_open(struct perf_event_attr *pe,
                             int cpu);

  /**
   * @brief Test on a specified CPU: try to measure multiple events simultaneously and determine whether the multiplexing is triggered.  
   * 
   * @param cpu_id CPU ID
   * @param event_num The number of events for this test
   * @return true Multiplexing is triggered
   * @return false Multiplexing is not triggered
   */
  bool test(uint64_t cpu_id, uint64_t event_num);

  bool enable_all_events();

  bool disable_all_events();

  void close_all_events();

  inline static const std::vector<std::pair<std::string, uint64_t>> event_list = {
      {"l1i_cache_refill", 0x0001},
      {"l1i_tlb_refill", 0x0002},
      {"l1d_cache_refill", 0x0003},
      {"l1d_cache", 0x0004},
      {"l1d_tlb_refill", 0x0005},
      {"ld_retired", 0x0006},
      {"st_retired", 0x0007},
      {"inst_retired", 0x0008},
      {"exc_taken", 0x0009},
      {"exc_return", 0x000a},
      {"cid_write_retired", 0x000b},
      {"pc_write_retired", 0x000c},
      {"br_immed_retired", 0x000d},
      {"br_return_retired", 0x000e},
      {"unaligned_ldst_retired", 0x000f},
      {"br_mis_pred", 0x0010},
      {"cpu_cycles", 0x0011},
      {"br_pred", 0x0012},
      {"mem_access", 0x0013},
      {"l1i_cache", 0x0014},
      {"l1d_cache_wb", 0x0015},
      {"l2d_cache", 0x0016},
      {"l2d_cache_refill", 0x0017},
      {"l2d_cache_wb", 0x0018},
      {"bus_access", 0x0019},
      {"memory_error", 0x001a},
      {"inst_spec", 0x001b},
      {"ttbr_write_retired", 0x001c},
      {"bus_cycles", 0x001d},
      /*{"chain", 0x001e}, [NOTE] This event may cause errors, do not use this for detection */
      {"l1d_cache_allocate", 0x001f},
      {"l2d_cache_allocate", 0x0020},
      {"br_retired", 0x0021}};
};