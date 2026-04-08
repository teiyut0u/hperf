#include <gtest/gtest.h>

#include <cstdint>

#include "hperf/read_buffer.h"

// GroupReadBuffer memory layout (with n events):
//   [0]      Header::nr            (uint64_t)
//   [1]      Header::time_enabled  (uint64_t)
//   [2]      Header::time_running  (uint64_t)
//   [3+2*i]  Entry[i]::value       (uint64_t)
//   [4+2*i]  Entry[i]::id          (uint64_t)

// --------------------------------------------------------- GroupReadBuffer

TEST(GroupReadBuffer, SizeMatchesLayout) {
  // n=2: 3 header fields + 2*2 entry fields = 7 uint64_t = 56 bytes
  EXPECT_EQ(GroupReadBuffer(2).size(), 56u);
}

TEST(GroupReadBuffer, SizeSingleEvent) {
  // n=1: 3 + 2 = 5 uint64_t = 40 bytes
  EXPECT_EQ(GroupReadBuffer(1).size(), 40u);
}

TEST(GroupReadBuffer, SizeZeroEvents) {
  // n=0: header only = 3 * 8 = 24 bytes
  EXPECT_EQ(GroupReadBuffer(0).size(), 24u);
}

TEST(GroupReadBuffer, ReadHeaderFields) {
  GroupReadBuffer buf(2);
  uint64_t* raw = static_cast<uint64_t*>(buf.data());
  raw[0] = 2;        // nr
  raw[1] = 1000000;  // time_enabled
  raw[2] = 900000;   // time_running

  EXPECT_EQ(buf.nr(), 2u);
  EXPECT_EQ(buf.time_enabled(), 1000000u);
  EXPECT_EQ(buf.time_running(), 900000u);
}

TEST(GroupReadBuffer, ReadEntries) {
  GroupReadBuffer buf(2);
  uint64_t* raw = static_cast<uint64_t*>(buf.data());
  raw[0] = 2;  // nr
  raw[1] = 1000000;
  raw[2] = 900000;
  raw[3] = 12345;   // entry[0].value
  raw[4] = 0xABCD;  // entry[0].id
  raw[5] = 67890;   // entry[1].value
  raw[6] = 0xEF01;  // entry[1].id

  auto e0 = buf.entry(0);
  ASSERT_TRUE(e0.has_value());
  EXPECT_EQ(e0->value, 12345u);
  EXPECT_EQ(e0->id, 0xABCDu);

  auto e1 = buf.entry(1);
  ASSERT_TRUE(e1.has_value());
  EXPECT_EQ(e1->value, 67890u);
  EXPECT_EQ(e1->id, 0xEF01u);
}

TEST(GroupReadBuffer, OutOfBoundsEntryReturnsNullopt) {
  GroupReadBuffer buf(2);
  uint64_t* raw = static_cast<uint64_t*>(buf.data());
  raw[0] = 2;  // nr = 2
  EXPECT_FALSE(buf.entry(2).has_value());
  EXPECT_FALSE(buf.entry(99).has_value());
}

TEST(GroupReadBuffer, ZeroNrMeansNoEntries) {
  GroupReadBuffer buf(1);
  uint64_t* raw = static_cast<uint64_t*>(buf.data());
  raw[0] = 0;  // nr = 0; the physical slot exists but entry() should reject it
  EXPECT_FALSE(buf.entry(0).has_value());
}

TEST(GroupReadBuffer, TimeMultiplexingDetectable) {
  // Multiplexing is present when time_enabled > time_running
  GroupReadBuffer buf(1);
  uint64_t* raw = static_cast<uint64_t*>(buf.data());
  raw[0] = 1;
  raw[1] = 1000000;  // time_enabled
  raw[2] = 500000;   // time_running (only ran half the time)

  EXPECT_GT(buf.time_enabled(), buf.time_running());
}

TEST(GroupReadBuffer, NoMultiplexingWhenTimesEqual) {
  GroupReadBuffer buf(1);
  uint64_t* raw = static_cast<uint64_t*>(buf.data());
  raw[0] = 1;
  raw[1] = 1000000;
  raw[2] = 1000000;  // time_enabled == time_running → no multiplexing

  EXPECT_EQ(buf.time_enabled(), buf.time_running());
}

// --------------------------------------------------------- SingleReadBuffer

TEST(SingleReadBuffer, SizeIs32Bytes) {
  // 4 fields * 8 bytes = 32 bytes
  EXPECT_EQ(SingleReadBuffer().size(), 32u);
}

TEST(SingleReadBuffer, DefaultZero) {
  SingleReadBuffer buf;
  EXPECT_EQ(buf.value(), 0u);
  EXPECT_EQ(buf.time_enabled(), 0u);
  EXPECT_EQ(buf.time_running(), 0u);
  EXPECT_EQ(buf.id(), 0u);
}

TEST(SingleReadBuffer, ReadAllFields) {
  SingleReadBuffer buf;
  uint64_t* raw = static_cast<uint64_t*>(buf.data());
  raw[0] = 99999;       // value
  raw[1] = 2000000;     // time_enabled
  raw[2] = 1500000;     // time_running
  raw[3] = 0xDEADBEEF;  // id

  EXPECT_EQ(buf.value(), 99999u);
  EXPECT_EQ(buf.time_enabled(), 2000000u);
  EXPECT_EQ(buf.time_running(), 1500000u);
  EXPECT_EQ(buf.id(), 0xDEADBEEFu);
}

TEST(SingleReadBuffer, TimeMultiplexingDetectable) {
  SingleReadBuffer buf;
  uint64_t* raw = static_cast<uint64_t*>(buf.data());
  raw[0] = 50000;
  raw[1] = 1000000;  // time_enabled
  raw[2] = 800000;   // time_running

  EXPECT_NE(buf.time_enabled(), buf.time_running());
}
