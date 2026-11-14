// Tests for drawing text modes in a host-supplied font, for a display too
// narrow for the adapter's own.
#include <gtest/gtest.h>

#include "test_helpers.h"
#include "video.h"

namespace {

using namespace video_test;

enum {
  // A 4x10 cell, which fits 80 columns across 320 pixels.
  kCellWidth = 4,
  kCellHeight = 10,
  kTextFrameWidth = 80 * kCellWidth,
  kTextFrameHeight = 25 * kCellHeight,

  // The CGA's frame, which graphics modes keep.
  kCGAFrameWidth = 640,
  kCGAFrameHeight = 200,

  // Glyphs the test font defines. Every other character is blank.
  kSolidBlock = 0xDB,
  kPatterned = 'A',
  // The patterned glyph's rows alternate between these, so that a pixel in
  // the wrong place shows.
  kPatternEvenRow = 0x9,
  kPatternOddRow = 0x6,

  // Mode control register values the BIOS writes for modes 1, 3 and 6.
  kControlText40 = 0x28,
  kControlText80 = 0x29,
  kControlGraphics640 = 0x1E,

  kColorBlack = 0,
  kColorRed = 4,
  kColorLightGreen = 10,
  kColorWhite = 15,

  // The 6845's cursor start register bit that turns the cursor off.
  kCursorStartDisable = 0x20,
};

// The font every test draws in.
struct TestFont {
  uint8_t glyphs[Y_VIDEO_TEXT_FONT_NUM_GLYPHS * kCellHeight] = {0};

  TestFont() {
    for (int row = 0; row < kCellHeight; ++row) {
      glyphs[kSolidBlock * kCellHeight + row] = 0xF;
      glyphs[kPatterned * kCellHeight + row] =
          row % 2 == 0 ? kPatternEvenRow : kPatternOddRow;
    }
  }

  YVideoTextFont Font() const {
    YVideoTextFont font = {};
    font.glyphs = glyphs;
    font.width = kCellWidth;
    font.height = kCellHeight;
    return font;
  }
};

const TestFont kTestFont;

class TextFontTest : public VideoTestBase {
 protected:
  void InitWithFont(YVideoAdapter adapter) {
    Init(adapter, kTestFont.Font());
    ASSERT_TRUE(video_.has_text_font);
    DisableCursor();
  }

  void SetControl(uint8_t value) {
    YVideoWritePort(&video_, Y_VIDEO_CGA_PORT_CONTROL, value);
  }

  YVideoRGB CGAColor(uint8_t index) const {
    return video_.config.cga_palette[index];
  }

  void ExpectFrameSize(int width, int height) {
    const YVideoSize frame = YVideoGetFrameSize(&video_);
    EXPECT_EQ(frame.width, width);
    EXPECT_EQ(frame.height, height);
  }

  // Put the cursor on a cell, spanning the given scan lines of the adapter's
  // own character cell.
  void PlaceCursor(uint16_t cell, uint8_t start, uint8_t end) {
    WriteRegister(Y_VIDEO_CRTC_REGISTER_CURSOR_START, start);
    WriteRegister(Y_VIDEO_CRTC_REGISTER_CURSOR_END, end);
    WriteRegister(
        Y_VIDEO_CRTC_REGISTER_CURSOR_H, static_cast<uint8_t>(cell >> 8));
    WriteRegister(Y_VIDEO_CRTC_REGISTER_CURSOR_L, static_cast<uint8_t>(cell));
  }
};

// ============================================================================
// Frame size
// ============================================================================

TEST_F(TextFontTest, WithoutAFontTheFrameIsTheAdapters) {
  Init(Y_VIDEO_ADAPTER_CGA);
  EXPECT_FALSE(video_.has_text_font);
  SetControl(kControlText80);
  ExpectFrameSize(kCGAFrameWidth, kCGAFrameHeight);
  SetControl(kControlGraphics640);
  ExpectFrameSize(kCGAFrameWidth, kCGAFrameHeight);
}

TEST_F(TextFontTest, TextModesTakeTheFontsCellSize) {
  InitWithFont(Y_VIDEO_ADAPTER_CGA);
  SetControl(kControlText80);
  ExpectFrameSize(kTextFrameWidth, kTextFrameHeight);
  // Half as many cells, each drawn twice as wide.
  SetControl(kControlText40);
  ExpectFrameSize(kTextFrameWidth, kTextFrameHeight);
}

TEST_F(TextFontTest, GraphicsModesKeepTheAdaptersFrame) {
  InitWithFont(Y_VIDEO_ADAPTER_CGA);
  SetControl(kControlGraphics640);
  ExpectFrameSize(kCGAFrameWidth, kCGAFrameHeight);

  Render();
  EXPECT_EQ(mock_pixel_write_count, kCGAFrameWidth * kCGAFrameHeight);
}

TEST_F(TextFontTest, TheMDAFrameTakesTheFontsCellSize) {
  InitWithFont(Y_VIDEO_ADAPTER_MDA);
  ExpectFrameSize(kTextFrameWidth, kTextFrameHeight);
}

// ============================================================================
// Rendering
// ============================================================================

TEST_F(TextFontTest, AFullRedrawCoversTheFontsFrame) {
  InitWithFont(Y_VIDEO_ADAPTER_CGA);
  SetControl(kControlText80);
  Render();

  ASSERT_EQ(mock_region_count, 1);
  EXPECT_EQ(mock_regions[0].origin.x, 0);
  EXPECT_EQ(mock_regions[0].origin.y, 0);
  EXPECT_EQ(mock_regions[0].width, kTextFrameWidth);
  EXPECT_EQ(mock_regions[0].height, kTextFrameHeight);
  EXPECT_EQ(mock_pixel_write_count, kTextFrameWidth * kTextFrameHeight);
}

TEST_F(TextFontTest, GlyphsComeFromTheFont) {
  InitWithFont(Y_VIDEO_ADAPTER_CGA);
  SetControl(kControlText80);
  const int kRow = 1;
  const int kCol = 2;
  WriteChar(kRow * 80 + kCol, kPatterned, kColorWhite);
  Render();

  const int x0 = kCol * kCellWidth;
  const int y0 = kRow * kCellHeight;
  for (int y = 0; y < kCellHeight; ++y) {
    const uint8_t bits = y % 2 == 0 ? kPatternEvenRow : kPatternOddRow;
    for (int x = 0; x < kCellWidth; ++x) {
      const bool lit = (bits & (1 << (kCellWidth - 1 - x))) != 0;
      EXPECT_EQ(
          Pixel(x0 + x, y0 + y), CGAColor(lit ? kColorWhite : kColorBlack))
          << "at (" << x << ", " << y << ") of the cell";
    }
  }
}

TEST_F(TextFontTest, AChangedCellRendersOnlyThatCell) {
  InitWithFont(Y_VIDEO_ADAPTER_CGA);
  SetControl(kControlText80);
  Render();

  const int kRow = 2;
  const int kCol = 3;
  WriteChar(kRow * 80 + kCol, kSolidBlock, kColorWhite);
  Render();

  ASSERT_EQ(mock_region_count, 1);
  EXPECT_EQ(mock_regions[0].origin.x, kCol * kCellWidth);
  EXPECT_EQ(mock_regions[0].origin.y, kRow * kCellHeight);
  EXPECT_EQ(mock_regions[0].width, kCellWidth);
  EXPECT_EQ(mock_regions[0].height, kCellHeight);
  EXPECT_EQ(mock_pixel_write_count, kCellWidth * kCellHeight);
  EXPECT_EQ(
      CountPixels(
          kCol * kCellWidth, kRow * kCellHeight, kCellWidth, kCellHeight,
          CGAColor(kColorWhite)),
      kCellWidth * kCellHeight);
}

TEST_F(TextFontTest, FortyColumnCellsAreTwiceTheFontsWidth) {
  InitWithFont(Y_VIDEO_ADAPTER_CGA);
  SetControl(kControlText40);
  WriteChar(0, kSolidBlock, kColorWhite);
  WriteChar(1, kSolidBlock, kColorRed);
  Render();

  EXPECT_EQ(
      CountPixels(0, 0, kCellWidth * 2, kCellHeight, CGAColor(kColorWhite)),
      kCellWidth * 2 * kCellHeight);
  EXPECT_EQ(
      CountPixels(
          kCellWidth * 2, 0, kCellWidth * 2, kCellHeight, CGAColor(kColorRed)),
      kCellWidth * 2 * kCellHeight);
}

TEST_F(TextFontTest, ASwitchToGraphicsAndBackRedrawsAtEachSize) {
  InitWithFont(Y_VIDEO_ADAPTER_CGA);
  SetControl(kControlText80);
  Render();
  EXPECT_EQ(mock_pixel_write_count, kTextFrameWidth * kTextFrameHeight);

  SetControl(kControlGraphics640);
  Render();
  EXPECT_EQ(mock_pixel_write_count, kCGAFrameWidth * kCGAFrameHeight);

  SetControl(kControlText80);
  Render();
  EXPECT_EQ(mock_pixel_write_count, kTextFrameWidth * kTextFrameHeight);
}

// ============================================================================
// Features placed by scan line
// ============================================================================

// The CGA's underline cursor covers scan lines 6 and 7 of 8, the bottom
// quarter of the cell. In a 10 row cell that is rows 7.5 to 10, so the rows it
// touches are 7 to 9.
TEST_F(TextFontTest, TheCursorScalesToTheFontsCellHeight) {
  InitWithFont(Y_VIDEO_ADAPTER_CGA);
  SetControl(kControlText80);
  WriteChar(2, ' ', kColorLightGreen);
  PlaceCursor(2, 6, 7);
  Render();

  const int x0 = 2 * kCellWidth;
  EXPECT_EQ(
      CountPixels(x0, 0, kCellWidth, 7, CGAColor(kColorBlack)), kCellWidth * 7);
  EXPECT_EQ(
      CountPixels(x0, 7, kCellWidth, 3, CGAColor(kColorLightGreen)),
      kCellWidth * 3);
}

TEST_F(TextFontTest, AFullHeightCursorFillsTheFontsCell) {
  InitWithFont(Y_VIDEO_ADAPTER_CGA);
  SetControl(kControlText80);
  WriteChar(0, ' ', kColorLightGreen);
  PlaceCursor(0, 0, 7);
  Render();

  EXPECT_EQ(
      CountPixels(0, 0, kCellWidth, kCellHeight, CGAColor(kColorLightGreen)),
      kCellWidth * kCellHeight);
}

TEST_F(TextFontTest, ACursorStartingPastTheCellStaysHidden) {
  InitWithFont(Y_VIDEO_ADAPTER_CGA);
  SetControl(kControlText80);
  WriteChar(0, ' ', kColorLightGreen);
  // Scan line 9 is past the CGA's 8 line cell, so no row of it is drawn.
  PlaceCursor(0, 9, 10);
  Render();

  EXPECT_EQ(
      CountPixels(0, 0, kCellWidth, kCellHeight, CGAColor(kColorBlack)),
      kCellWidth * kCellHeight);
}

// The MDA underlines on scan line 12 of 14, the second from the bottom. In a
// 10 row cell that is row 8, the second from the bottom there too.
TEST_F(TextFontTest, TheMDAUnderlineScalesToTheFontsCellHeight) {
  InitWithFont(Y_VIDEO_ADAPTER_MDA);
  // A blank character with the underline attribute.
  WriteChar(0, ' ', 0x01);
  Render();

  for (int y = 0; y < kCellHeight; ++y) {
    const YVideoRGB expected =
        y == 8 ? video_.config.foreground : video_.config.background;
    EXPECT_EQ(CountPixels(0, y, kCellWidth, 1, expected), kCellWidth)
        << "on row " << y;
  }
}

// The MDA's cursor covers scan lines 11 and 12 of 14, which in a 10 row cell
// touch rows 7 to 9.
TEST_F(TextFontTest, TheMDACursorScalesToTheFontsCellHeight) {
  InitWithFont(Y_VIDEO_ADAPTER_MDA);
  PlaceCursor(0, 11, 12);
  Render();

  EXPECT_EQ(
      CountPixels(0, 0, kCellWidth, 7, video_.config.background),
      kCellWidth * 7);
  EXPECT_EQ(
      CountPixels(0, 7, kCellWidth, 3, video_.config.foreground),
      kCellWidth * 3);
}

// ============================================================================
// Fonts the renderer cannot draw
// ============================================================================

TEST_F(TextFontTest, AFontWiderThanAByteIsRefused) {
  YVideoTextFont font = kTestFont.Font();
  font.width = Y_VIDEO_TEXT_FONT_MAX_WIDTH + 1;
  Init(Y_VIDEO_ADAPTER_CGA, font);

  EXPECT_FALSE(video_.has_text_font);
  ExpectFrameSize(kCGAFrameWidth, kCGAFrameHeight);
  EXPECT_EQ(mock_log_error_count, 1);
  // Consumed, so that the fixture's check for unexpected errors passes.
  mock_log_error_count = 0;
}

TEST_F(TextFontTest, AFontWithNoHeightIsRefused) {
  YVideoTextFont font = kTestFont.Font();
  font.height = 0;
  Init(Y_VIDEO_ADAPTER_CGA, font);

  EXPECT_FALSE(video_.has_text_font);
  EXPECT_EQ(mock_log_error_count, 1);
  mock_log_error_count = 0;
}

// The default config leaves the glyphs NULL, which is no font rather than a
// broken one, whatever the size fields say.
TEST_F(TextFontTest, NoGlyphsIsNoFontAndNotAnError) {
  YVideoTextFont font = kTestFont.Font();
  font.glyphs = nullptr;
  Init(Y_VIDEO_ADAPTER_CGA, font);

  EXPECT_FALSE(video_.has_text_font);
  ExpectFrameSize(kCGAFrameWidth, kCGAFrameHeight);
}

}  // namespace
