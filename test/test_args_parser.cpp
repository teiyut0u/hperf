#include <gtest/gtest.h>

#include <cstring>
#include <vector>

#include "hperf/args_parser.h"

// Helper: build argc/argv from initializer list of C strings.
// getopt_long requires non-const char**, so we copy each string.
class ArgsParserTest : public ::testing::Test {
 protected:
  ArgsParser parser;
  ProfileConfig config;

  void SetUp() override {
    // Reset getopt global state between tests.
    // On glibc, optind = 0 triggers full re-initialization of internal state.
    optind = 0;
  }

  bool parse(std::initializer_list<const char *> args) {
    argv_storage_.clear();
    argv_ptrs_.clear();
    for (const char *arg : args) {
      argv_storage_.emplace_back(arg);
    }
    for (auto &s : argv_storage_) {
      argv_ptrs_.push_back(s.data());
    }
    argv_ptrs_.push_back(nullptr);
    return parser.parse(config, static_cast<int>(argv_storage_.size()), argv_ptrs_.data());
  }

 private:
  std::vector<std::string> argv_storage_;
  std::vector<char *> argv_ptrs_;
};

// ========================= Normal parsing =========================

TEST_F(ArgsParserTest, SystemWideWithDuration) {
  EXPECT_TRUE(parse({"hperf", "-a", "-d", "5"}));
  EXPECT_EQ(config.mode, ProfileMode::SYSTEM_WIDE);
  EXPECT_EQ(config.test_duration, 5);
}

TEST_F(ArgsParserTest, SystemWideWithCpuList) {
  EXPECT_TRUE(parse({"hperf", "-a", "-d", "5", "-c", "0,2-4"}));
  EXPECT_EQ(config.mode, ProfileMode::SYSTEM_WIDE);
  std::vector<int> expected = {0, 2, 3, 4};
  EXPECT_EQ(config.cpu_id_list, expected);
}

TEST_F(ArgsParserTest, PerProcessMode) {
  EXPECT_TRUE(parse({"hperf", "-p", "1234"}));
  EXPECT_EQ(config.mode, ProfileMode::TRACK_PID);
  EXPECT_EQ(config.target_pid, 1234);
}

TEST_F(ArgsParserTest, SubprocessMode) {
  EXPECT_TRUE(parse({"hperf", "/bin/sleep", "10"}));
  EXPECT_EQ(config.mode, ProfileMode::SUBPROCESS);
  ASSERT_GE(config.command_args.size(), 3u);
  EXPECT_STREQ(config.command_args[0], "/bin/sleep");
  EXPECT_STREQ(config.command_args[1], "10");
  EXPECT_EQ(config.command_args[2], nullptr);
}

TEST_F(ArgsParserTest, IntervalOption) {
  EXPECT_TRUE(parse({"hperf", "-a", "-d", "5", "-i", "500"}));
  EXPECT_EQ(config.switch_group_interval, 500);
}

TEST_F(ArgsParserTest, OutputFileOption) {
  EXPECT_TRUE(parse({"hperf", "-a", "-d", "5", "-o", "out.csv"}));
  EXPECT_EQ(config.output_filename, "out.csv");
}

TEST_F(ArgsParserTest, UserModeSched) {
  EXPECT_TRUE(parse({"hperf", "-a", "-d", "5", "--user-mode-sched"}));
  EXPECT_TRUE(config.user_mode_sched);
}

// ========================= Early exit options =========================

TEST_F(ArgsParserTest, HelpFlag) {
  EXPECT_TRUE(parse({"hperf", "-h"}));
  EXPECT_TRUE(config.help_requested);
}

TEST_F(ArgsParserTest, DetectCounters) {
  EXPECT_TRUE(parse({"hperf", "--detect-counters"}));
  EXPECT_TRUE(config.detect_counters);
}

TEST_F(ArgsParserTest, ListEvents) {
  EXPECT_TRUE(parse({"hperf", "--list-events"}));
  EXPECT_TRUE(config.list_events);
}

// ========================= Invalid numeric values =========================

TEST_F(ArgsParserTest, InvalidDuration_NonNumeric) {
  EXPECT_FALSE(parse({"hperf", "-a", "-d", "abc"}));
}

TEST_F(ArgsParserTest, InvalidDuration_Zero) {
  EXPECT_FALSE(parse({"hperf", "-a", "-d", "0"}));
}

TEST_F(ArgsParserTest, InvalidDuration_Negative) {
  EXPECT_FALSE(parse({"hperf", "-a", "-d", "-1"}));
}

TEST_F(ArgsParserTest, InvalidDuration_TrailingChars) {
  EXPECT_FALSE(parse({"hperf", "-a", "-d", "5abc"}));
}

TEST_F(ArgsParserTest, InvalidInterval_NonNumeric) {
  EXPECT_FALSE(parse({"hperf", "-a", "-d", "5", "-i", "abc"}));
}

TEST_F(ArgsParserTest, InvalidInterval_Zero) {
  EXPECT_FALSE(parse({"hperf", "-a", "-d", "5", "-i", "0"}));
}

TEST_F(ArgsParserTest, InvalidPid_NonNumeric) {
  EXPECT_FALSE(parse({"hperf", "-p", "abc"}));
}

TEST_F(ArgsParserTest, InvalidPid_Zero) {
  EXPECT_FALSE(parse({"hperf", "-p", "0"}));
}

TEST_F(ArgsParserTest, InvalidPid_Negative) {
  EXPECT_FALSE(parse({"hperf", "-p", "-1"}));
}

// ========================= Mode conflicts =========================

TEST_F(ArgsParserTest, ConflictSystemWideAndPid) {
  EXPECT_FALSE(parse({"hperf", "-a", "-d", "5", "-p", "1234"}));
}

TEST_F(ArgsParserTest, ConflictSystemWideAndCommand) {
  EXPECT_FALSE(parse({"hperf", "-a", "-d", "5", "/bin/sleep", "1"}));
}

TEST_F(ArgsParserTest, ConflictPidAndCommand) {
  EXPECT_FALSE(parse({"hperf", "-p", "1234", "/bin/sleep", "1"}));
}

TEST_F(ArgsParserTest, NoModeSpecified) {
  EXPECT_FALSE(parse({"hperf"}));
}

TEST_F(ArgsParserTest, SystemWideWithoutDuration) {
  EXPECT_FALSE(parse({"hperf", "-a"}));
}

// ========================= Monitor mode =========================

TEST_F(ArgsParserTest, MonitorModeValid) {
  EXPECT_TRUE(parse({"hperf", "--monitor", "arm_cmn_mem_bw_all", "--cmn-mc-pos", "mc_pos.txt"}));
  EXPECT_EQ(config.monitor_target, MonitorTarget::ARM_CMN_MEM_BW_ALL);
  EXPECT_EQ(config.mc_position_file, "mc_pos.txt");
}

TEST_F(ArgsParserTest, MonitorModeInvalidTarget) {
  EXPECT_FALSE(parse({"hperf", "--monitor", "invalid_target", "--cmn-mc-pos", "mc_pos.txt"}));
}

TEST_F(ArgsParserTest, MonitorModeMissingMcPos) {
  EXPECT_FALSE(parse({"hperf", "--monitor", "arm_cmn_mem_bw_all"}));
}

TEST_F(ArgsParserTest, MonitorModeConflictWithSystemWide) {
  EXPECT_FALSE(parse({"hperf", "--monitor", "arm_cmn_mem_bw_all", "--cmn-mc-pos", "mc_pos.txt", "-a", "-d", "5"}));
}

// ========================= CPU list format =========================

TEST_F(ArgsParserTest, CpuListSingleValue) {
  EXPECT_TRUE(parse({"hperf", "-a", "-d", "5", "-c", "3"}));
  std::vector<int> expected = {3};
  EXPECT_EQ(config.cpu_id_list, expected);
}

TEST_F(ArgsParserTest, CpuListMultipleValues) {
  EXPECT_TRUE(parse({"hperf", "-a", "-d", "5", "-c", "0,1,3"}));
  std::vector<int> expected = {0, 1, 3};
  EXPECT_EQ(config.cpu_id_list, expected);
}

TEST_F(ArgsParserTest, CpuListRange) {
  EXPECT_TRUE(parse({"hperf", "-a", "-d", "5", "-c", "2-5"}));
  std::vector<int> expected = {2, 3, 4, 5};
  EXPECT_EQ(config.cpu_id_list, expected);
}

TEST_F(ArgsParserTest, CpuListMixed) {
  EXPECT_TRUE(parse({"hperf", "-a", "-d", "5", "-c", "0,2-4,7"}));
  std::vector<int> expected = {0, 2, 3, 4, 7};
  EXPECT_EQ(config.cpu_id_list, expected);
}

TEST_F(ArgsParserTest, CpuListInvalid_NonNumeric) {
  EXPECT_FALSE(parse({"hperf", "-a", "-d", "5", "-c", "abc"}));
}

TEST_F(ArgsParserTest, CpuListInvalid_EmptyToken) {
  EXPECT_FALSE(parse({"hperf", "-a", "-d", "5", "-c", "1,,3"}));
}

TEST_F(ArgsParserTest, CpuListInvalid_ReversedRange) {
  EXPECT_FALSE(parse({"hperf", "-a", "-d", "5", "-c", "5-3"}));
}
