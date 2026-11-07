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
    Init(Y_VIDEO_ADAPTER_CGA);
    // The cursor sits over the first character cell by default, which is the
    // cell most of these tests inspect.
    DisableCursor();
  }

  void SetControl(uint8_t value) {
    YVideoWritePort(&video_, Y_VIDEO_CGA_PORT_CONTROL, value);
  }

  void SetColorSelect(uint8_t value) {
    YVideoWritePort(&video_, Y_VIDEO_CGA_PORT_COLOR_SELECT, value);
  }

  YVideoRGB Color(uint8_t index) const {
    return video_.config.cga_palette[index];
  }

  // Write a byte of graphics mode VRAM. Even scan lines live in the first half
  // of VRAM and odd scan lines in the second.
  void WriteGraphicsByte(int y, int x_byte, uint8_t value) {
    uint32_t address = (y % 2 ? Y_VIDEO_CGA_GRAPHICS_ODD_SCAN_LINE_OFFSET : 0) +
                       (y / 2) * Y_VIDEO_CGA_GRAPHICS_BYTES_PER_SCAN_LINE +
                       x_byte;
    YVideoWriteVRAM(&video_, address, value);
  }
};

// ============================================================================
// Initialization and mode selection
// ============================================================================

TEST_F(CGATest, Initialization) {
  EXPECT_EQ(video_.adapter, Y_VIDEO_ADAPTER_CGA);
  EXPECT_EQ(YVideoGetMode(&video_), Y_VIDEO_MODE_CGA_TEXT_80X25_COLOR);
  EXPECT_EQ(video_.control_register, 0x29);
  EXPECT_EQ(video_.registers[Y_VIDEO_CRTC_REGISTER_HORIZONTAL_TOTAL], 0x71);
  EXPECT_EQ(mock_vram[0], ' ');
  EXPECT_EQ(mock_vram[1], 0x07);
  EXPECT_EQ(mock_vram[Y_VIDEO_CGA_VRAM_SIZE - 2], ' ');
  EXPECT_EQ(mock_vram[Y_VIDEO_CGA_VRAM_SIZE - 1], 0x07);
}

TEST_F(CGATest, AdapterMetadata) {
  const YVideoAdapterMetadata* adapter = YVideoGetAdapterMetadata(&video_);
  EXPECT_EQ(adapter->adapter, Y_VIDEO_ADAPTER_CGA);
  EXPECT_EQ(adapter->frame_buffer_width, kFrameBufferWidth);
  EXPECT_EQ(adapter->frame_buffer_height, kFrameBufferHeight);
  EXPECT_EQ(
      adapter->vram_address, static_cast<uint32_t>(Y_VIDEO_CGA_VRAM_ADDRESS));
  EXPECT_EQ(adapter->vram_size, static_cast<uint32_t>(Y_VIDEO_CGA_VRAM_SIZE));
  EXPECT_EQ(adapter->port_start, Y_VIDEO_CGA_PORT_START);
  EXPECT_EQ(adapter->port_end, Y_VIDEO_CGA_PORT_END);
}

TEST_F(CGATest, ModeSelection) {
  struct ModeCase {
    uint8_t control;
    YVideoMode mode;
  };
  const ModeCase cases[] = {
      {kControlMode0, Y_VIDEO_MODE_CGA_TEXT_40X25_MONO},
      {kControlMode1, Y_VIDEO_MODE_CGA_TEXT_40X25_COLOR},
      {kControlMode2, Y_VIDEO_MODE_CGA_TEXT_80X25_MONO},
      {kControlMode3, Y_VIDEO_MODE_CGA_TEXT_80X25_COLOR},
      {kControlMode4, Y_VIDEO_MODE_CGA_GRAPHICS_320X200},
      {kControlMode5, Y_VIDEO_MODE_CGA_GRAPHICS_320X200_ALT},
      {kControlMode6, Y_VIDEO_MODE_CGA_GRAPHICS_640X200},
  };
  for (const ModeCase& mode_case : cases) {
    SetControl(mode_case.control);
    EXPECT_EQ(YVideoGetMode(&video_), mode_case.mode)
        << "control register " << static_cast<int>(mode_case.control);
    EXPECT_EQ(YVideoGetModeMetadata(&video_)->mode, mode_case.mode);
  }
}

TEST_F(CGATest, ModeControlBitsAreResolvedInHardwareOrder) {
  // The BIOS never writes these combinations, but software that ORs a bit into
  // a saved mode byte can. A real card resolves the high resolution bit before
  // the graphics bits, so 80x25 text wins over graphics.
  SetControl(kControlMode3 | Y_VIDEO_CONTROL_GRAPHICS);
  EXPECT_EQ(YVideoGetMode(&video_), Y_VIDEO_MODE_CGA_TEXT_80X25_COLOR);
  SetControl(kControlMode3 | Y_VIDEO_CONTROL_HIGH_RESOLUTION_GRAPHICS);
  EXPECT_EQ(YVideoGetMode(&video_), Y_VIDEO_MODE_CGA_TEXT_80X25_COLOR);

  // And the graphics bit before the high resolution graphics bit, so a set high
  // resolution graphics bit means nothing on its own.
  SetControl(kControlMode1 | Y_VIDEO_CONTROL_HIGH_RESOLUTION_GRAPHICS);
  EXPECT_EQ(YVideoGetMode(&video_), Y_VIDEO_MODE_CGA_TEXT_40X25_COLOR);
}

// ============================================================================
// I/O ports
// ============================================================================

TEST_F(CGATest, PortReadWrite) {
  YVideoWritePort(
      &video_, Y_VIDEO_CGA_PORT_REGISTER_INDEX, Y_VIDEO_CRTC_REGISTER_CURSOR_L);
  EXPECT_EQ(video_.selected_register, Y_VIDEO_CRTC_REGISTER_CURSOR_L);
  YVideoWritePort(&video_, Y_VIDEO_CGA_PORT_REGISTER_DATA, 0x42);
  EXPECT_EQ(video_.registers[Y_VIDEO_CRTC_REGISTER_CURSOR_L], 0x42);
  EXPECT_EQ(YVideoReadPort(&video_, Y_VIDEO_CGA_PORT_REGISTER_DATA), 0x42);

  SetColorSelect(0x3F);
  EXPECT_EQ(video_.color_select_register, 0x3F);
  EXPECT_EQ(YVideoReadPort(&video_, Y_VIDEO_CGA_PORT_COLOR_SELECT), 0x3F);
}

TEST_F(CGATest, RegisterPortsAreAliased) {
  // The CGA decodes only the low address bits, so 3D0 to 3D7 all reach the
  // 6845's index and data registers.
  YVideoWritePort(
      &video_, Y_VIDEO_CGA_PORT_START, Y_VIDEO_CRTC_REGISTER_CURSOR_H);
  EXPECT_EQ(video_.selected_register, Y_VIDEO_CRTC_REGISTER_CURSOR_H);
  YVideoWritePort(&video_, Y_VIDEO_CGA_PORT_START + 3, 0x07);
  EXPECT_EQ(video_.registers[Y_VIDEO_CRTC_REGISTER_CURSOR_H], 0x07);
  EXPECT_EQ(YVideoReadPort(&video_, Y_VIDEO_CGA_PORT_START + 7), 0x07);
}

TEST_F(CGATest, RegisterIndexIsMaskedToFiveBits) {
  YVideoWritePort(&video_, Y_VIDEO_CGA_PORT_REGISTER_INDEX, 0xE1);
  EXPECT_EQ(video_.selected_register, 0x01);
}

TEST_F(CGATest, LightPenPortsAreInert) {
  YVideoWritePort(&video_, Y_VIDEO_CGA_PORT_CLEAR_LIGHT_PEN, 0xFF);
  YVideoWritePort(&video_, Y_VIDEO_CGA_PORT_PRESET_LIGHT_PEN, 0xFF);
  EXPECT_EQ(YVideoReadPort(&video_, Y_VIDEO_CGA_PORT_CLEAR_LIGHT_PEN), 0xFF);
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
  EXPECT_EQ(
      mock_pixel_span_count,
      (kFrameBufferWidth / Y_VIDEO_PIXEL_BATCH_SIZE) * kFrameBufferHeight);
}

TEST_F(CGATest, RenderText40x25IsPixelDoubled) {
  SetControl(kControlMode1);
  DisableCursor();
  ASSERT_EQ(YVideoGetMode(&video_), Y_VIDEO_MODE_CGA_TEXT_40X25_COLOR);

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

  video_.dirty_state.ranges[1].start_column = 5;
  video_.dirty_state.ranges[1].end_column = 8;
  video_.dirty_state.status = Y_VIDEO_DIRTY;
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
  AdvanceFrames(Y_VIDEO_FRAMES_PER_TEXT_BLINK_PHASE);
  Render();
  EXPECT_EQ(
      CountPixels(0, 0, kCharWidth, kCharHeight, Color(kColorBlue)),
      kCharWidth * kCharHeight);
}

TEST_F(CGATest, IntenseBackgroundWhenBlinkIsDisabled) {
  SetControl(kControlMode3 & ~Y_VIDEO_CONTROL_ENABLE_BLINK);
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
  WriteRegister(Y_VIDEO_CRTC_REGISTER_CURSOR_START, 0x06);
  WriteRegister(Y_VIDEO_CRTC_REGISTER_CURSOR_END, 0x07);
  // Put the cursor on the third cell of the first row, in light green.
  WriteChar(2, ' ', kColorLightGreen);
  WriteRegister(Y_VIDEO_CRTC_REGISTER_CURSOR_H, 0);
  WriteRegister(Y_VIDEO_CRTC_REGISTER_CURSOR_L, 2);
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
  WriteRegister(Y_VIDEO_CRTC_REGISTER_CURSOR_START, 0x06);
  WriteRegister(Y_VIDEO_CRTC_REGISTER_CURSOR_END, 0x07);
  WriteRegister(Y_VIDEO_CRTC_REGISTER_CURSOR_H, 0);
  WriteRegister(Y_VIDEO_CRTC_REGISTER_CURSOR_L, 0);
  WriteChar(0, ' ', 0x80 | (kColorBlue << 4) | kColorWhite);
  Render();

  AdvanceFrames(Y_VIDEO_FRAMES_PER_TEXT_BLINK_PHASE);
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

  WriteRegister(Y_VIDEO_CRTC_REGISTER_START_ADDRESS_H, kPageSize >> 8);
  WriteRegister(Y_VIDEO_CRTC_REGISTER_START_ADDRESS_L, kPageSize & 0xFF);
  Render();
  EXPECT_EQ(Pixel(0, 0), Color(kColorWhite));
}

// ============================================================================
// Graphics mode 4 / 5 - 320x200, four colors
// ============================================================================

TEST_F(CGATest, RenderGraphics320x200) {
  SetControl(kControlMode4);
  ASSERT_EQ(YVideoGetMode(&video_), Y_VIDEO_MODE_CGA_GRAPHICS_320X200);
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
  EXPECT_EQ(mock_vram[Y_VIDEO_CGA_GRAPHICS_ODD_SCAN_LINE_OFFSET], 0xFF);
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
      {kControlMode4, Y_VIDEO_CGA_COLOR_SELECT_PALETTE, kColorCyan,
       kColorLightGray},
      // Palette 1, high intensity.
      {kControlMode4,
       Y_VIDEO_CGA_COLOR_SELECT_PALETTE |
           Y_VIDEO_CGA_COLOR_SELECT_PALETTE_INTENSITY,
       kColorLightCyan, kColorWhite},
      // Black and white overrides the palette bit: cyan, red, light gray.
      {kControlMode5, Y_VIDEO_CGA_COLOR_SELECT_PALETTE, kColorCyan,
       kColorLightGray},
      // Black and white at high intensity.
      {kControlMode5, Y_VIDEO_CGA_COLOR_SELECT_PALETTE_INTENSITY,
       kColorLightCyan, kColorWhite},
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

  SetColorSelect(kColorMagenta | Y_VIDEO_CGA_COLOR_SELECT_INTENSITY);
  Render();
  EXPECT_EQ(Pixel(0, 0), Color(kColorLightMagenta));
}

// ============================================================================
// Graphics mode 6 - 640x200, two colors
// ============================================================================

TEST_F(CGATest, RenderGraphics640x200) {
  SetControl(kControlMode6);
  ASSERT_EQ(YVideoGetMode(&video_), Y_VIDEO_MODE_CGA_GRAPHICS_640X200);
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
  EXPECT_EQ(mock_pixel_span_count, 4);
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
  WriteRegister(Y_VIDEO_CRTC_REGISTER_START_ADDRESS_H, 0);
  WriteRegister(Y_VIDEO_CRTC_REGISTER_START_ADDRESS_L, 40);
  Render();
  EXPECT_EQ(Pixel(0, 0), Color(kColorWhite));
}

TEST_F(CGATest, VideoEnableBlanksTheDisplay) {
  SetControl(kControlMode3);
  WriteChar(0, 0xDB, kColorWhite);
  SetControl(kControlMode3 & ~Y_VIDEO_CONTROL_VIDEO_ENABLE);
  Render();

  EXPECT_EQ(
      CountPixels(
          0, 0, kFrameBufferWidth, kFrameBufferHeight, Color(kColorBlack)),
      kFrameBufferWidth * kFrameBufferHeight);
}

}  // namespace
