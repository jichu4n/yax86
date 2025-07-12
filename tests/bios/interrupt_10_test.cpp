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

TEST_F(Interrupt10Test, AH03ReadCursorPosition) {
  BIOSTestHelper helper;

  // Test reading cursor position after setting it
  // First, set cursor position to (row=10, col=20) on page 0
  uint8_t ah = 0x02;  // Set cursor position
  uint8_t dh = 10;    // Row 10
  uint8_t dl = 20;    // Column 20
  uint8_t bh = 0;     // Page 0
  helper.cpu_.registers[kAX] = (ah << 8);
  helper.cpu_.registers[kDX] = (dh << 8) | dl;
  helper.cpu_.registers[kBX] = (bh << 8);
  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);

  // Now read cursor position
  ah = 0x03;  // Read cursor position
  bh = 0;     // Page 0
  helper.cpu_.registers[kAX] = (ah << 8);
  helper.cpu_.registers[kBX] = (bh << 8);
  helper.cpu_.registers[kDX] = 0;  // Clear DX register
  helper.cpu_.registers[kCX] = 0;  // Clear CX register
  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);

  // Verify cursor position was read correctly
  uint8_t returned_dh = (helper.cpu_.registers[kDX] >> 8) & 0xFF;
  uint8_t returned_dl = helper.cpu_.registers[kDX] & 0xFF;
  EXPECT_EQ(returned_dh, 10);  // Row should be 10
  EXPECT_EQ(returned_dl, 20);  // Column should be 20

  // Verify cursor shape was read correctly (default MDA cursor: start=12,
  // end=13)
  uint8_t returned_ch = (helper.cpu_.registers[kCX] >> 8) & 0xFF;
  uint8_t returned_cl = helper.cpu_.registers[kCX] & 0xFF;
  EXPECT_EQ(returned_ch, 12);  // Start row should be 12 (char_height-2 for MDA)
  EXPECT_EQ(returned_cl, 13);  // End row should be 13 (char_height-1 for MDA)

  // Test reading cursor position with custom cursor shape
  // Set a custom cursor shape first
  ah = 0x01;       // Set cursor shape
  uint8_t ch = 5;  // Cursor start row
  uint8_t cl = 7;  // Cursor end row
  helper.cpu_.registers[kAX] = (ah << 8);
  helper.cpu_.registers[kCX] = (ch << 8) | cl;
  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);

  // Read cursor position again to verify custom cursor shape is returned
  ah = 0x03;  // Read cursor position
  bh = 0;     // Page 0
  helper.cpu_.registers[kAX] = (ah << 8);
  helper.cpu_.registers[kBX] = (bh << 8);
  helper.cpu_.registers[kDX] = 0;  // Clear DX register
  helper.cpu_.registers[kCX] = 0;  // Clear CX register
  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);

  // Verify cursor position is still the same
  returned_dh = (helper.cpu_.registers[kDX] >> 8) & 0xFF;
  returned_dl = helper.cpu_.registers[kDX] & 0xFF;
  EXPECT_EQ(returned_dh, 10);  // Row should still be 10
  EXPECT_EQ(returned_dl, 20);  // Column should still be 20

  // Verify custom cursor shape is returned
  returned_ch = (helper.cpu_.registers[kCX] >> 8) & 0xFF;
  returned_cl = helper.cpu_.registers[kCX] & 0xFF;
  EXPECT_EQ(returned_ch, 5);  // Start row should be 5
  EXPECT_EQ(returned_cl, 7);  // End row should be 7

  // Test reading cursor position at screen boundaries
  // Set cursor to bottom-right corner
  ah = 0x02;  // Set cursor position
  dh = 24;    // Row 24 (last row for MDA 80x25)
  dl = 79;    // Column 79 (last column for MDA 80x25)
  bh = 0;     // Page 0
  helper.cpu_.registers[kAX] = (ah << 8);
  helper.cpu_.registers[kDX] = (dh << 8) | dl;
  helper.cpu_.registers[kBX] = (bh << 8);
  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);

  // Read cursor position at boundary
  ah = 0x03;  // Read cursor position
  bh = 0;     // Page 0
  helper.cpu_.registers[kAX] = (ah << 8);
  helper.cpu_.registers[kBX] = (bh << 8);
  helper.cpu_.registers[kDX] = 0;  // Clear DX register
  helper.cpu_.registers[kCX] = 0;  // Clear CX register
  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);

  // Verify boundary cursor position was read correctly
  returned_dh = (helper.cpu_.registers[kDX] >> 8) & 0xFF;
  returned_dl = helper.cpu_.registers[kDX] & 0xFF;
  EXPECT_EQ(returned_dh, 24);  // Row should be 24
  EXPECT_EQ(returned_dl, 79);  // Column should be 79

  // Test reading cursor position at origin (0,0)
  // Set cursor to origin
  ah = 0x02;  // Set cursor position
  dh = 0;     // Row 0
  dl = 0;     // Column 0
  bh = 0;     // Page 0
  helper.cpu_.registers[kAX] = (ah << 8);
  helper.cpu_.registers[kDX] = (dh << 8) | dl;
  helper.cpu_.registers[kBX] = (bh << 8);
  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);

  // Read cursor position at origin
  ah = 0x03;  // Read cursor position
  bh = 0;     // Page 0
  helper.cpu_.registers[kAX] = (ah << 8);
  helper.cpu_.registers[kBX] = (bh << 8);
  helper.cpu_.registers[kDX] =
      0x1234;  // Set to non-zero to verify it gets cleared
  helper.cpu_.registers[kCX] = 0x5678;  // Set to non-zero to verify it gets set
  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);

  // Verify origin cursor position was read correctly
  returned_dh = (helper.cpu_.registers[kDX] >> 8) & 0xFF;
  returned_dl = helper.cpu_.registers[kDX] & 0xFF;
  EXPECT_EQ(returned_dh, 0);  // Row should be 0
  EXPECT_EQ(returned_dl, 0);  // Column should be 0

  // Verify cursor shape is still returned
  returned_ch = (helper.cpu_.registers[kCX] >> 8) & 0xFF;
  returned_cl = helper.cpu_.registers[kCX] & 0xFF;
  EXPECT_EQ(returned_ch, 5);  // Start row should still be 5 (custom shape)
  EXPECT_EQ(returned_cl, 7);  // End row should still be 7 (custom shape)
}

TEST_F(Interrupt10Test, AH05SetActiveDisplayPage) {
  BIOSTestHelper helper;

  // Test setting page 0 (valid page for MDA)
  uint8_t ah = 0x05;
  uint8_t al = 0;  // Page 0
  helper.cpu_.registers[kAX] = (ah << 8) | al;
  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);
  EXPECT_EQ(TextGetCurrentPage(&helper.bios_), 0);

  // Test setting page 1 (invalid page for MDA - should remain at page 0)
  ah = 0x05;
  al = 1;  // Page 1 (invalid for MDA)
  helper.cpu_.registers[kAX] = (ah << 8) | al;
  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);
  EXPECT_EQ(TextGetCurrentPage(&helper.bios_), 0);  // Should remain at page 0

  // Verify that cursor positions are maintained correctly for page 0
  // First, set a cursor position
  ah = 0x02;        // Set cursor position
  uint8_t dh = 10;  // Row 10
  uint8_t dl = 20;  // Column 20
  uint8_t bh = 0;   // Page 0
  helper.cpu_.registers[kAX] = (ah << 8);
  helper.cpu_.registers[kDX] = (dh << 8) | dl;
  helper.cpu_.registers[kBX] = (bh << 8);
  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);

  // Set active page to 0 again
  ah = 0x05;
  al = 0;  // Page 0
  helper.cpu_.registers[kAX] = (ah << 8) | al;
  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);
  EXPECT_EQ(TextGetCurrentPage(&helper.bios_), 0);

  // Verify cursor position is still preserved
  TextPosition cursor_pos = TextGetCursorPositionForPage(&helper.bios_, 0);
  EXPECT_EQ(cursor_pos.row, 10);
  EXPECT_EQ(cursor_pos.col, 20);
}

TEST_F(Interrupt10Test, AH06ScrollActivePageUp) {
  BIOSTestHelper helper;
  const VideoModeMetadata* metadata =
      GetCurrentVideoModeMetadata(&helper.bios_);
  const uint32_t vram_base = metadata->vram_address;
  const int cols = metadata->columns;
  const int rows = metadata->rows;

  // --- Test 1: Scroll up a portion of the screen ---
  // Fill a 3x3 area with characters 'A', 'B', 'C' on separate lines
  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      uint32_t offset = vram_base + (row * cols + col) * 2;
      WriteMemoryByte(&helper.bios_, offset, 'A' + row);
      WriteMemoryByte(&helper.bios_, offset + 1, 0x07);
    }
  }

  // Scroll the 3x3 area up by 1 line
  uint8_t ah = 0x06;
  uint8_t al = 1;     // Scroll 1 line up
  uint8_t ch = 0;     // Start row
  uint8_t cl = 0;     // Start col
  uint8_t dh = 2;     // End row
  uint8_t dl = 2;     // End col
  uint8_t bh = 0x70;  // Attribute for blank line (grey on black)
  helper.cpu_.registers[kAX] = (ah << 8) | al;
  helper.cpu_.registers[kCX] = (ch << 8) | cl;
  helper.cpu_.registers[kDX] = (dh << 8) | dl;
  helper.cpu_.registers[kBX] = (bh << 8);

  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);

  // Verify that the first row is now the original second row ('B')
  for (int col = 0; col < 3; ++col) {
    uint32_t offset = vram_base + (0 * cols + col) * 2;
    EXPECT_EQ(ReadMemoryByte(&helper.bios_, offset), 'B');
  }
  // Verify that the second row is now the original third row ('C')
  for (int col = 0; col < 3; ++col) {
    uint32_t offset = vram_base + (1 * cols + col) * 2;
    EXPECT_EQ(ReadMemoryByte(&helper.bios_, offset), 'C');
  }
  // Verify that the third row is now blank with the specified attribute
  for (int col = 0; col < 3; ++col) {
    uint32_t offset = vram_base + (2 * cols + col) * 2;
    EXPECT_EQ(ReadMemoryByte(&helper.bios_, offset), ' ');
    EXPECT_EQ(ReadMemoryByte(&helper.bios_, offset + 1), bh);
  }

  // --- Test 2: Clear a region by setting AL to 0 ---
  // First, fill a region with some data
  for (int row = 5; row < 8; ++row) {
    for (int col = 5; col < 8; ++col) {
      uint32_t offset = vram_base + (row * cols + col) * 2;
      WriteMemoryByte(&helper.bios_, offset, 'X');
      WriteMemoryByte(&helper.bios_, offset + 1, 0x1F);
    }
  }

  // Now, clear the region
  ah = 0x06;
  al = 0;     // Clear region
  ch = 5;     // Start row
  cl = 5;     // Start col
  dh = 7;     // End row
  dl = 7;     // End col
  bh = 0x07;  // Attribute for blank (white on black)
  helper.cpu_.registers[kAX] = (ah << 8) | al;
  helper.cpu_.registers[kCX] = (ch << 8) | cl;
  helper.cpu_.registers[kDX] = (dh << 8) | dl;
  helper.cpu_.registers[kBX] = (bh << 8);

  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);

  // Verify the region is cleared
  for (int row = 5; row < 8; ++row) {
    for (int col = 5; col < 8; ++col) {
      uint32_t offset = vram_base + (row * cols + col) * 2;
      EXPECT_EQ(ReadMemoryByte(&helper.bios_, offset), ' ');
      EXPECT_EQ(ReadMemoryByte(&helper.bios_, offset + 1), bh);
    }
  }

  // --- Test 3: Scroll the entire screen up by 5 lines ---
  // Fill the screen with a pattern
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      uint32_t offset = vram_base + (row * cols + col) * 2;
      WriteMemoryByte(&helper.bios_, offset, 'A' + row);
      WriteMemoryByte(&helper.bios_, offset + 1, 0x0F);
    }
  }

  // Scroll the entire screen (80x25) up by 5 lines
  ah = 0x06;
  al = 5;         // Scroll 5 lines up
  ch = 0;         // Start row
  cl = 0;         // Start col
  dh = rows - 1;  // End row
  dl = cols - 1;  // End col
  bh = 0x1E;      // Attribute for blank line (yellow on blue)
  helper.cpu_.registers[kAX] = (ah << 8) | al;
  helper.cpu_.registers[kCX] = (ch << 8) | cl;
  helper.cpu_.registers[kDX] = (dh << 8) | dl;
  helper.cpu_.registers[kBX] = (bh << 8);

  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);

  // Verify that the content has shifted up by 5 lines
  for (int row = 0; row < rows - 5; ++row) {
    for (int col = 0; col < cols; ++col) {
      uint32_t offset = vram_base + (row * cols + col) * 2;
      // Original content was from row+5
      EXPECT_EQ(ReadMemoryByte(&helper.bios_, offset), 'A' + (row + 5));
      EXPECT_EQ(ReadMemoryByte(&helper.bios_, offset + 1), 0x0F);
    }
  }

  // Verify that the bottom 5 lines are now blank with the new attribute
  for (int row = rows - 5; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      uint32_t offset = vram_base + (row * cols + col) * 2;
      EXPECT_EQ(ReadMemoryByte(&helper.bios_, offset), ' ');
      EXPECT_EQ(ReadMemoryByte(&helper.bios_, offset + 1), bh);
    }
  }

  // --- Test 4: Clear the entire screen by scrolling 25 lines ---
  // Fill the screen with a pattern again
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      uint32_t offset = vram_base + (row * cols + col) * 2;
      WriteMemoryByte(&helper.bios_, offset, 'Z' - row);
      WriteMemoryByte(&helper.bios_, offset + 1, 0x2F);
    }
  }

  // Scroll the entire screen up by 25 lines (should clear it)
  ah = 0x06;
  al = rows;      // Scroll `rows` lines up
  ch = 0;         // Start row
  cl = 0;         // Start col
  dh = rows - 1;  // End row
  dl = cols - 1;  // End col
  bh = 0x07;      // Attribute for blank (white on black)
  helper.cpu_.registers[kAX] = (ah << 8) | al;
  helper.cpu_.registers[kCX] = (ch << 8) | cl;
  helper.cpu_.registers[kDX] = (dh << 8) | dl;
  helper.cpu_.registers[kBX] = (bh << 8);

  EXPECT_EQ(
      HandleBIOSInterrupt(&helper.bios_, &helper.cpu_, 0x10), kExecuteSuccess);

  // Verify that the entire screen is now blank
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      uint32_t offset = vram_base + (row * cols + col) * 2;
      EXPECT_EQ(ReadMemoryByte(&helper.bios_, offset), ' ');
      EXPECT_EQ(ReadMemoryByte(&helper.bios_, offset + 1), bh);
    }
  }
}
