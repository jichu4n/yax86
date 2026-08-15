#include <gtest/gtest.h>

#include "test_helpers.h"
#include "video.h"

namespace {

using namespace video_test;

enum {
  // The CGA frame buffer is the same size in every mode.
  kFrameBufferWidth = 640,
  kFrameBufferHeight = 200,
  kCharWidth = 8,
  kCharHeight = 8,

  // Mode control register values written by the BIOS for each of its modes,
  // from the CRT_MODE table in GLaBIOS.
  kControlMode0 = 0x2C,
  kControlMode1 = 0x28,
  kControlMode2 = 0x2D,
  kControlMode3 = 0x29,
  kControlMode4 = 0x2A,
  kControlMode5 = 0x2E,
  kControlMode6 = 0x1E,

  // Colors in the standard CGA palette.
  kColorBlack = 0,
  kColorBlue = 1,
  kColorGreen = 2,
  kColorCyan = 3,
  kColorRed = 4,
  kColorMagenta = 5,
  kColorBrown = 6,
  kColorLightGray = 7,
  kColorLightGreen = 10,
  kColorLightCyan = 11,
  kColorLightRed = 12,
  kColorLightMagenta = 13,
  kColorWhite = 15,
};

class CGATest : public VideoTestBase {
 protected:
  void SetUp() override {
    Init(kVideoAdapterCGA);
    // The cursor sits over the first character cell by default, which is the
    // cell most of these tests inspect.
    DisableCursor();
  }

  void SetControl(uint8_t value) {
    VideoWritePort(&video_, kCGAPortControl, value);
  }

  void SetColorSelect(uint8_t value) {
    VideoWritePort(&video_, kCGAPortColorSelect, value);
  }

  RGB Color(uint8_t index) const { return config_.cga_palette[index]; }

  // Write a byte of graphics mode VRAM. Even scan lines live in the first half
  // of VRAM and odd scan lines in the second.
  void WriteGraphicsByte(int y, int x_byte, uint8_t value) {
    uint32_t address = (y % 2 ? kCGAGraphicsOddScanLineOffset : 0) +
                       (y / 2) * kCGAGraphicsBytesPerScanLine + x_byte;
    VideoWriteVRAM(&video_, address, value);
  }
};

// ============================================================================
// Initialization and mode selection
// ============================================================================

TEST_F(CGATest, Initialization) {
  EXPECT_EQ(video_.adapter, kVideoAdapterCGA);
  EXPECT_EQ(VideoGetMode(&video_), kVideoModeCGAText80x25Color);
  EXPECT_EQ(video_.control_register, 0x29);
  EXPECT_EQ(video_.registers[kCRTCRegisterHorizontalTotal], 0x71);
  EXPECT_EQ(mock_vram[0], ' ');
  EXPECT_EQ(mock_vram[1], 0x07);
  EXPECT_EQ(mock_vram[kCGAVRAMSize - 2], ' ');
  EXPECT_EQ(mock_vram[kCGAVRAMSize - 1], 0x07);
}

TEST_F(CGATest, AdapterMetadata) {
  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(&video_);
  EXPECT_EQ(adapter->adapter, kVideoAdapterCGA);
  EXPECT_EQ(adapter->frame_buffer_width, kFrameBufferWidth);
  EXPECT_EQ(adapter->frame_buffer_height, kFrameBufferHeight);
  EXPECT_EQ(adapter->vram_address, static_cast<uint32_t>(kCGAVRAMAddress));
  EXPECT_EQ(adapter->vram_size, static_cast<uint32_t>(kCGAVRAMSize));
  EXPECT_EQ(adapter->port_start, kCGAPortStart);
  EXPECT_EQ(adapter->port_end, kCGAPortEnd);
}

TEST_F(CGATest, ModeSelection) {
  struct ModeCase {
    uint8_t control;
    VideoMode mode;
  };
  const ModeCase cases[] = {
      {kControlMode0, kVideoModeCGAText40x25Mono},
      {kControlMode1, kVideoModeCGAText40x25Color},
      {kControlMode2, kVideoModeCGAText80x25Mono},
      {kControlMode3, kVideoModeCGAText80x25Color},
      {kControlMode4, kVideoModeCGAGraphics320x200},
      {kControlMode5, kVideoModeCGAGraphics320x200Alt},
      {kControlMode6, kVideoModeCGAGraphics640x200},
  };
  for (const ModeCase& mode_case : cases) {
    SetControl(mode_case.control);
    EXPECT_EQ(VideoGetMode(&video_), mode_case.mode)
        << "control register " << static_cast<int>(mode_case.control);
    EXPECT_EQ(VideoGetModeMetadata(&video_)->mode, mode_case.mode);
  }
}

TEST_F(CGATest, ModeControlBitsAreResolvedInHardwareOrder) {
  // The BIOS never writes these combinations, but software that ORs a bit into
  // a saved mode byte can. A real card resolves the high resolution bit before
  // the graphics bits, so 80x25 text wins over graphics.
  SetControl(kControlMode3 | kVideoControlGraphics);
  EXPECT_EQ(VideoGetMode(&video_), kVideoModeCGAText80x25Color);
  SetControl(kControlMode3 | kVideoControlHighResolutionGraphics);
  EXPECT_EQ(VideoGetMode(&video_), kVideoModeCGAText80x25Color);

  // And the graphics bit before the high resolution graphics bit, so a set high
  // resolution graphics bit means nothing on its own.
  SetControl(kControlMode1 | kVideoControlHighResolutionGraphics);
  EXPECT_EQ(VideoGetMode(&video_), kVideoModeCGAText40x25Color);
}

// ============================================================================
// I/O ports
// ============================================================================

TEST_F(CGATest, PortReadWrite) {
  VideoWritePort(&video_, kCGAPortRegisterIndex, kCRTCRegisterCursorL);
  EXPECT_EQ(video_.selected_register, kCRTCRegisterCursorL);
  VideoWritePort(&video_, kCGAPortRegisterData, 0x42);
  EXPECT_EQ(video_.registers[kCRTCRegisterCursorL], 0x42);
  EXPECT_EQ(VideoReadPort(&video_, kCGAPortRegisterData), 0x42);

  SetColorSelect(0x3F);
  EXPECT_EQ(video_.color_select_register, 0x3F);
  EXPECT_EQ(VideoReadPort(&video_, kCGAPortColorSelect), 0x3F);
}

TEST_F(CGATest, RegisterPortsAreAliased) {
  // The CGA decodes only the low address bits, so 3D0 to 3D7 all reach the
  // 6845's index and data registers.
  VideoWritePort(&video_, kCGAPortStart, kCRTCRegisterCursorH);
  EXPECT_EQ(video_.selected_register, kCRTCRegisterCursorH);
  VideoWritePort(&video_, kCGAPortStart + 3, 0x07);
  EXPECT_EQ(video_.registers[kCRTCRegisterCursorH], 0x07);
  EXPECT_EQ(VideoReadPort(&video_, kCGAPortStart + 7), 0x07);
}

TEST_F(CGATest, RegisterIndexIsMaskedToFiveBits) {
  VideoWritePort(&video_, kCGAPortRegisterIndex, 0xE1);
  EXPECT_EQ(video_.selected_register, 0x01);
}

TEST_F(CGATest, LightPenPortsAreInert) {
  VideoWritePort(&video_, kCGAPortClearLightPen, 0xFF);
  VideoWritePort(&video_, kCGAPortPresetLightPen, 0xFF);
  EXPECT_EQ(VideoReadPort(&video_, kCGAPortClearLightPen), 0xFF);
}

// ============================================================================
// Text modes
// ============================================================================

TEST_F(CGATest, RenderText80x25) {
  SetControl(kControlMode3);
  // 'A' in light red on blue.
  WriteChar(0, 'A', (kColorBlue << 4) | kColorLightRed);
  Render();

  int foreground_pixels =
      CountPixels(0, 0, kCharWidth, kCharHeight, Color(kColorLightRed));
  int background_pixels =
      CountPixels(0, 0, kCharWidth, kCharHeight, Color(kColorBlue));
  EXPECT_GT(foreground_pixels, 0);
  EXPECT_GT(background_pixels, 0);
  EXPECT_EQ(foreground_pixels + background_pixels, kCharWidth * kCharHeight);
  // The whole frame buffer is covered.
  EXPECT_EQ(mock_pixel_write_count, kFrameBufferWidth * kFrameBufferHeight);
}

TEST_F(CGATest, RenderText40x25IsPixelDoubled) {
  SetControl(kControlMode1);
  DisableCursor();
  ASSERT_EQ(VideoGetMode(&video_), kVideoModeCGAText40x25Color);

  // A solid block character in white on black.
  WriteChar(0, 0xDB, kColorWhite);
  Render();

  // The cell is twice as wide on screen as the 8 pixel wide glyph.
  EXPECT_EQ(
      CountPixels(0, 0, kCharWidth * 2, kCharHeight, Color(kColorWhite)),
      kCharWidth * 2 * kCharHeight);
  // The second cell starts right after it.
  WriteChar(1, 0xDB, kColorRed);
  Render();
  EXPECT_EQ(Pixel(kCharWidth * 2, 0), Color(kColorRed));
  EXPECT_EQ(mock_pixel_write_count, kCharWidth * 2 * kCharHeight);
}

TEST_F(CGATest, TextWriteRendersOnlyTheCellsCharacterRow) {
  SetControl(kControlMode3);
  Render();

  const uint8_t kCol = 3;
  const uint8_t kRow = 2;
  WriteChar(kRow * 80 + kCol, 'A', kColorWhite);
  Render();

  ASSERT_EQ(mock_region_count, 1);
  EXPECT_EQ(mock_region_end_count, 1);
  EXPECT_EQ(mock_regions[0].origin.x, kCol * kCharWidth);
  EXPECT_EQ(mock_regions[0].origin.y, kRow * kCharHeight);
  EXPECT_EQ(mock_regions[0].width, kCharWidth);
  EXPECT_EQ(mock_regions[0].height, kCharHeight);
  EXPECT_EQ(mock_pixel_write_count, kCharWidth * kCharHeight);

  // The retained display needs no work when neither VRAM nor registers change.
  Render();
  EXPECT_EQ(mock_region_count, 0);
  EXPECT_EQ(mock_pixel_write_count, 0);
}

TEST_F(CGATest, DirtyRangeSplittingA40ColumnCellIsReported) {
  // In 40-column text a character cell spans two dirty columns, so the
  // renderer can only draw whole pairs. A range starting or ending mid-cell
  // would emit pixels the region it declared does not cover - which on a
  // display addressed by transfer window displaces every pixel after it.
  //
  // Every invalidation path scales a character column by two, so no caller
  // produces such a range today. This reaches around them to confirm that if
  // one ever does, it is reported rather than silently corrupting the display.
  SetControl(kControlMode0);
  Render();

  video_.dirty.ranges[1].first_column = 5;
  video_.dirty.ranges[1].end_column = 8;
  video_.dirty.any_dirty = true;
  Render();

  ASSERT_EQ(mock_region_count, 1);
  // Three dirty columns of eight pixels are declared to the host, starting at
  // pixel 40.
  EXPECT_EQ(mock_regions[0].origin.x, 5 * kCharWidth);
  EXPECT_EQ(mock_regions[0].width, 3 * kCharWidth);
  // Truncating both ends to whole cells renders cells 2 and 3 instead, which
  // is 32 pixels wide and starts at pixel 32 - both more pixels than the
  // region declared and some of them outside it.
  EXPECT_EQ(mock_pixel_write_count, 4 * kCharWidth * kCharHeight);
  EXPECT_EQ(mock_region_area_mismatches, 1);
  EXPECT_GT(mock_pixels_outside_region, 0);
  EXPECT_EQ(mock_log_error_count, 1);

  // Consume the deliberate mismatch so the fixture's teardown check passes.
  mock_region_area_mismatches = 0;
  mock_pixels_outside_region = 0;
  mock_log_error_count = 0;
}

TEST_F(CGATest, BlinkingAttributeLimitsBackgroundColors) {
  SetControl(kControlMode3);
  DisableCursor();
  // Attribute bit 7 set with blink enabled means blinking, not an intense
  // background, so the background stays plain blue.
  WriteChar(0, 'A', 0x80 | (kColorBlue << 4) | kColorWhite);
  Render();
  EXPECT_GT(CountPixels(0, 0, kCharWidth, kCharHeight, Color(kColorBlue)), 0);
  EXPECT_EQ(
      CountPixels(0, 0, kCharWidth, kCharHeight, Color(kColorBlue + 8)), 0);

  // In the opposite blink phase the character disappears into its background.
  AdvanceFrames(kVideoFramesPerTextBlinkPhase);
  Render();
  EXPECT_EQ(
      CountPixels(0, 0, kCharWidth, kCharHeight, Color(kColorBlue)),
      kCharWidth * kCharHeight);
}

TEST_F(CGATest, IntenseBackgroundWhenBlinkIsDisabled) {
  SetControl(kControlMode3 & ~kVideoControlEnableBlink);
  DisableCursor();
  // With blink disabled, attribute bit 7 is the background intensity, giving
  // all sixteen background colors.
  WriteChar(0, 'A', 0x80 | (kColorBlue << 4) | kColorWhite);
  Render();

  EXPECT_GT(
      CountPixels(0, 0, kCharWidth, kCharHeight, Color(kColorBlue + 8)), 0);
  EXPECT_EQ(CountPixels(0, 0, kCharWidth, kCharHeight, Color(kColorBlue)), 0);
}

TEST_F(CGATest, RenderCursor) {
  SetControl(kControlMode3);
  WriteRegister(kCRTCRegisterCursorStart, 0x06);
  WriteRegister(kCRTCRegisterCursorEnd, 0x07);
  // Put the cursor on the third cell of the first row, in light green.
  WriteChar(2, ' ', kColorLightGreen);
  WriteRegister(kCRTCRegisterCursorH, 0);
  WriteRegister(kCRTCRegisterCursorL, 2);
  Render();

  int cursor_x = 2 * kCharWidth;
  EXPECT_EQ(
      CountPixels(cursor_x, 6, kCharWidth, 2, Color(kColorLightGreen)),
      kCharWidth * 2);
  EXPECT_EQ(
      CountPixels(cursor_x, 0, kCharWidth, 6, Color(kColorBlack)),
      kCharWidth * 6);
}

TEST_F(CGATest, CursorKeepsItsColorWhenBlinkingCharacterIsHidden) {
  SetControl(kControlMode3);
  WriteRegister(kCRTCRegisterCursorStart, 0x06);
  WriteRegister(kCRTCRegisterCursorEnd, 0x07);
  WriteRegister(kCRTCRegisterCursorH, 0);
  WriteRegister(kCRTCRegisterCursorL, 0);
  WriteChar(0, ' ', 0x80 | (kColorBlue << 4) | kColorWhite);
  Render();

  AdvanceFrames(kVideoFramesPerTextBlinkPhase);
  Render();

  EXPECT_EQ(
      CountPixels(0, 6, kCharWidth, 2, Color(kColorWhite)), kCharWidth * 2);
  EXPECT_EQ(
      CountPixels(0, 0, kCharWidth, 6, Color(kColorBlue)), kCharWidth * 6);
}

TEST_F(CGATest, StartAddressSelectsThePage) {
  SetControl(kControlMode3);
  DisableCursor();
  // The BIOS switches text pages by pointing the 6845 at a different part of
  // VRAM. Page 1 of an 80x25 screen starts 0x800 characters in.
  const uint16_t kPageSize = 0x800;
  WriteChar(kPageSize, 0xDB, kColorWhite);
  Render();
  EXPECT_EQ(Pixel(0, 0), Color(kColorBlack));

  WriteRegister(kCRTCRegisterStartAddressH, kPageSize >> 8);
  WriteRegister(kCRTCRegisterStartAddressL, kPageSize & 0xFF);
  Render();
  EXPECT_EQ(Pixel(0, 0), Color(kColorWhite));
}

// ============================================================================
// Graphics mode 4 / 5 - 320x200, four colors
// ============================================================================

TEST_F(CGATest, RenderGraphics320x200) {
  SetControl(kControlMode4);
  ASSERT_EQ(VideoGetMode(&video_), kVideoModeCGAGraphics320x200);
  // One byte holds four pixels, most significant bits leftmost.
  WriteGraphicsByte(0, 0, 0x1B);  // 00 01 10 11
  Render();

  // Palette 0 at low intensity: background, green, red, brown. Every pixel is
  // doubled horizontally into the 640 pixel wide frame buffer.
  EXPECT_EQ(Pixel(0, 0), Color(kColorBlack));
  EXPECT_EQ(Pixel(1, 0), Color(kColorBlack));
  EXPECT_EQ(Pixel(2, 0), Color(kColorGreen));
  EXPECT_EQ(Pixel(3, 0), Color(kColorGreen));
  EXPECT_EQ(Pixel(4, 0), Color(kColorRed));
  EXPECT_EQ(Pixel(5, 0), Color(kColorRed));
  EXPECT_EQ(Pixel(6, 0), Color(kColorBrown));
  EXPECT_EQ(Pixel(7, 0), Color(kColorBrown));
  EXPECT_EQ(mock_pixel_write_count, kFrameBufferWidth * kFrameBufferHeight);
}

TEST_F(CGATest, Graphics320x200ScanLinesAreInterleaved) {
  SetControl(kControlMode4);
  // Scan line 1 lives in the second half of VRAM. WriteGraphicsByte does that
  // mapping, so writing there must land on the second row of the display.
  WriteGraphicsByte(1, 0, 0xFF);
  Render();

  EXPECT_EQ(Pixel(0, 0), Color(kColorBlack));
  EXPECT_EQ(Pixel(0, 1), Color(kColorBrown));
  // And the raw address it used really is in the second half.
  EXPECT_EQ(mock_vram[kCGAGraphicsOddScanLineOffset], 0xFF);
}

TEST_F(CGATest, Graphics320x200Palettes) {
  // 11 in every pixel, so each test sees palette entry 3.
  WriteGraphicsByte(0, 0, 0xFF);
  // And 01 for palette entry 1.
  WriteGraphicsByte(0, 1, 0x55);

  struct PaletteCase {
    uint8_t control;
    uint8_t color_select;
    uint8_t expected_color_1;
    uint8_t expected_color_3;
  };
  const PaletteCase cases[] = {
      // Palette 0, low intensity: green, red, brown.
      {kControlMode4, 0x00, kColorGreen, kColorBrown},
      // Palette 1, low intensity: cyan, magenta, light gray.
      {kControlMode4, kCGAColorSelectPalette, kColorCyan, kColorLightGray},
      // Palette 1, high intensity.
      {kControlMode4, kCGAColorSelectPalette | kCGAColorSelectPaletteIntensity,
       kColorLightCyan, kColorWhite},
      // Black and white overrides the palette bit: cyan, red, light gray.
      {kControlMode5, kCGAColorSelectPalette, kColorCyan, kColorLightGray},
      // Black and white at high intensity.
      {kControlMode5, kCGAColorSelectPaletteIntensity, kColorLightCyan,
       kColorWhite},
  };
  for (const PaletteCase& palette_case : cases) {
    SetControl(palette_case.control);
    SetColorSelect(palette_case.color_select);
    Render();
    EXPECT_EQ(Pixel(0, 0), Color(palette_case.expected_color_3))
        << "control " << static_cast<int>(palette_case.control)
        << " color select " << static_cast<int>(palette_case.color_select);
    EXPECT_EQ(Pixel(8, 0), Color(palette_case.expected_color_1))
        << "control " << static_cast<int>(palette_case.control)
        << " color select " << static_cast<int>(palette_case.color_select);
  }
}

TEST_F(CGATest, Graphics320x200BackgroundColor) {
  SetControl(kControlMode4);
  // Color 0 comes from the low four bits of the color select register.
  SetColorSelect(kColorMagenta);
  Render();
  EXPECT_EQ(Pixel(0, 0), Color(kColorMagenta));

  SetColorSelect(kColorMagenta | kCGAColorSelectIntensity);
  Render();
  EXPECT_EQ(Pixel(0, 0), Color(kColorLightMagenta));
}

// ============================================================================
// Graphics mode 6 - 640x200, two colors
// ============================================================================

TEST_F(CGATest, RenderGraphics640x200) {
  SetControl(kControlMode6);
  ASSERT_EQ(VideoGetMode(&video_), kVideoModeCGAGraphics640x200);
  // One byte holds eight pixels, most significant bit leftmost. The BIOS leaves
  // the color select register at white for this mode.
  SetColorSelect(kColorWhite);
  WriteGraphicsByte(0, 0, 0xA0);  // 1010 0000
  Render();

  EXPECT_EQ(Pixel(0, 0), Color(kColorWhite));
  EXPECT_EQ(Pixel(1, 0), Color(kColorBlack));
  EXPECT_EQ(Pixel(2, 0), Color(kColorWhite));
  EXPECT_EQ(Pixel(3, 0), Color(kColorBlack));
  EXPECT_EQ(Pixel(7, 0), Color(kColorBlack));
  // No pixel doubling in this mode.
  EXPECT_EQ(mock_pixel_write_count, kFrameBufferWidth * kFrameBufferHeight);
}

TEST_F(CGATest, GraphicsWriteRendersItsFourLineGroup) {
  SetControl(kControlMode6);
  SetColorSelect(kColorWhite);
  Render();

  WriteGraphicsByte(5, 7, 0x80);
  Render();

  ASSERT_EQ(mock_region_count, 1);
  EXPECT_EQ(mock_regions[0].origin.x, 7 * 8);
  EXPECT_EQ(mock_regions[0].origin.y, 4);
  EXPECT_EQ(mock_regions[0].width, 8);
  EXPECT_EQ(mock_regions[0].height, 4);
  EXPECT_EQ(mock_pixel_write_count, 8 * 4);
  EXPECT_EQ(Pixel(7 * 8, 5), Color(kColorWhite));
}

TEST_F(CGATest, GraphicsWritesShareTheRangeOfTheirFourLineGroup) {
  SetControl(kControlMode6);
  SetColorSelect(kColorWhite);
  Render();

  WriteGraphicsByte(4, 2, 0x80);
  WriteGraphicsByte(7, 6, 0x01);
  Render();

  ASSERT_EQ(mock_region_count, 1);
  EXPECT_EQ(mock_regions[0].origin.x, 2 * 8);
  EXPECT_EQ(mock_regions[0].origin.y, 4);
  EXPECT_EQ(mock_regions[0].width, 5 * 8);
  EXPECT_EQ(mock_regions[0].height, 4);
}

TEST_F(CGATest, Graphics640x200ForegroundColor) {
  SetControl(kControlMode6);
  SetColorSelect(kColorCyan);
  WriteGraphicsByte(0, 0, 0x80);
  Render();

  EXPECT_EQ(Pixel(0, 0), Color(kColorCyan));
  EXPECT_EQ(Pixel(1, 0), Color(kColorBlack));
}

TEST_F(CGATest, Graphics640x200ScanLinesAreInterleaved) {
  SetControl(kControlMode6);
  SetColorSelect(kColorWhite);
  WriteGraphicsByte(3, 0, 0x80);
  Render();

  EXPECT_EQ(Pixel(0, 3), Color(kColorWhite));
  EXPECT_EQ(Pixel(0, 2), Color(kColorBlack));
}

TEST_F(CGATest, GraphicsStartAddressScrollsTheDisplay) {
  SetControl(kControlMode6);
  SetColorSelect(kColorWhite);
  // Scan line 2 is the second row of the even half of VRAM, 80 bytes in.
  WriteGraphicsByte(2, 0, 0x80);
  Render();
  EXPECT_EQ(Pixel(0, 0), Color(kColorBlack));
  EXPECT_EQ(Pixel(0, 2), Color(kColorWhite));

  // The 6845 counts in character units and a graphics mode fetches two bytes
  // per unit, so a start address of 40 moves the display on by 80 bytes - which
  // is exactly the row that was scan line 2.
  WriteRegister(kCRTCRegisterStartAddressH, 0);
  WriteRegister(kCRTCRegisterStartAddressL, 40);
  Render();
  EXPECT_EQ(Pixel(0, 0), Color(kColorWhite));
}

TEST_F(CGATest, VideoEnableBlanksTheDisplay) {
  SetControl(kControlMode3);
  WriteChar(0, 0xDB, kColorWhite);
  SetControl(kControlMode3 & ~kVideoControlVideoEnable);
  Render();

  EXPECT_EQ(
      CountPixels(
          0, 0, kFrameBufferWidth, kFrameBufferHeight, Color(kColorBlack)),
      kFrameBufferWidth * kFrameBufferHeight);
}

}  // namespace
