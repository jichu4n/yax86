#include <gtest/gtest.h>

#include "bios.h"
#include "bios_test_helper.h"

using namespace std;

class Interrupt10Test : public ::testing::Test {};

TEST_F(Interrupt10Test, AH00SetVideoMode) {
  BIOSTestHelper helper;

  // Test switching to supported video mode.
  uint8_t ah = 0x00;
  uint8_t al = kVideoTextModeMDA07;
  helper.cpu_.registers[kAX] = (ah << 8) | al;
  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);
  EXPECT_EQ(GetCurrentVideoMode(&helper.bios_), kVideoTextModeMDA07);

  // Test switching to unsupported video mode.
  ah = 0x00;
  al = 0x42;  // Invalid video mode
  helper.cpu_.registers[kAX] = (ah << 8) | al;
  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);
  EXPECT_EQ(GetCurrentVideoMode(&helper.bios_), kVideoTextModeMDA07);
}

TEST_F(Interrupt10Test, AH02SetCursorPosition) {
  BIOSTestHelper helper;

  // Test setting cursor position on page 0
  uint8_t ah = 0x02;
  uint8_t dh = 5;   // Row 5
  uint8_t dl = 10;  // Column 10
  uint8_t bh = 0;   // Page 0
  helper.cpu_.registers[kAX] = (ah << 8);
  helper.cpu_.registers[kDX] = (dh << 8) | dl;
  helper.cpu_.registers[kBX] = (bh << 8);
  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);
  TextPosition cursor_pos = TextGetCursorPositionForPage(&helper.bios_, 0);
  EXPECT_EQ(cursor_pos.row, 5);
  EXPECT_EQ(cursor_pos.col, 10);

  // Test setting cursor position on page 0 (different location)
  ah = 0x02;
  dh = 12;  // Row 12
  dl = 25;  // Column 25
  bh = 0;   // Page 0 (only valid page for MDA)
  helper.cpu_.registers[kAX] = (ah << 8);
  helper.cpu_.registers[kDX] = (dh << 8) | dl;
  helper.cpu_.registers[kBX] = (bh << 8);
  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);
  cursor_pos = TextGetCursorPositionForPage(&helper.bios_, 0);
  EXPECT_EQ(cursor_pos.row, 12);
  EXPECT_EQ(cursor_pos.col, 25);

  // Test setting cursor position at screen boundaries (MDA: 80x25 text mode)
  ah = 0x02;
  dh = 24;  // Row 24 (last row, 0-indexed)
  dl = 79;  // Column 79 (last column, 0-indexed)
  bh = 0;   // Page 0
  helper.cpu_.registers[kAX] = (ah << 8);
  helper.cpu_.registers[kDX] = (dh << 8) | dl;
  helper.cpu_.registers[kBX] = (bh << 8);
  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);
  cursor_pos = TextGetCursorPositionForPage(&helper.bios_, 0);
  EXPECT_EQ(cursor_pos.row, 24);
  EXPECT_EQ(cursor_pos.col, 79);

  // Test setting cursor position outside page boundaries
  ah = 0x02;
  dh = 100;  // Row 100
  dl = 0;    // Column 0
  bh = 0;    // Page 0
  helper.cpu_.registers[kAX] = (ah << 8);
  helper.cpu_.registers[kDX] = (dh << 8) | dl;
  helper.cpu_.registers[kBX] = (bh << 8);
  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);
  cursor_pos = TextGetCursorPositionForPage(&helper.bios_, 0);
  EXPECT_EQ(cursor_pos.row, 24);
  EXPECT_EQ(cursor_pos.col, 79);
}
