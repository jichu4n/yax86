#include <gtest/gtest.h>

#include "test_helpers.h"
#include "video.h"

namespace {

using namespace video_test;

enum {
  kCharWidth = 9,
  kCharHeight = 14,
  // Scan line within a character cell that the underline occupies.
  kUnderlinePosition = 12,
};

class MDATest : public VideoTestBase {
 protected:
  void SetUp() override {
    Init(kVideoAdapterMDA);
    // The cursor sits over the first character cell by default, which is the
    // cell most of these tests inspect.
    DisableCursor();
  }

  // Number of pixels of the given color within the first character cell.
  int CountPixelsInFirstCell(RGB rgb) const {
    return CountPixels(0, 0, kCharWidth, kCharHeight, rgb);
  }
};

TEST_F(MDATest, Initialization) {
  EXPECT_EQ(video_.adapter, kVideoAdapterMDA);
  EXPECT_EQ(VideoGetMode(&video_), kVideoModeMDAText80x25);
  EXPECT_EQ(video_.control_register, 0x29);
  EXPECT_EQ(video_.selected_register, 0);
  EXPECT_EQ(video_.registers[kCRTCRegisterHorizontalTotal], 0x61);
  // Verify VRAM was cleared to spaces with the default attribute.
  EXPECT_EQ(mock_vram[0], ' ');
  EXPECT_EQ(mock_vram[1], 0x07);
  EXPECT_EQ(mock_vram[kMDAVRAMSize - 2], ' ');
  EXPECT_EQ(mock_vram[kMDAVRAMSize - 1], 0x07);
}

TEST_F(MDATest, AdapterMetadata) {
  const VideoAdapterMetadata* adapter = VideoGetAdapterMetadata(&video_);
  EXPECT_EQ(adapter->adapter, kVideoAdapterMDA);
  EXPECT_EQ(adapter->frame_buffer_width, 720);
  EXPECT_EQ(adapter->frame_buffer_height, 350);
  EXPECT_EQ(adapter->vram_address, static_cast<uint32_t>(kMDAVRAMAddress));
  EXPECT_EQ(adapter->vram_size, static_cast<uint32_t>(kMDAVRAMSize));
  EXPECT_EQ(adapter->port_start, kMDAPortStart);
  EXPECT_EQ(adapter->port_end, kMDAPortEnd);
}

TEST_F(MDATest, PortReadWrite) {
  // Index register.
  VideoWritePort(&video_, kMDAPortRegisterIndex, kCRTCRegisterHorizontalTotal);
  EXPECT_EQ(video_.selected_register, kCRTCRegisterHorizontalTotal);
  EXPECT_EQ(
      VideoReadPort(&video_, kMDAPortRegisterIndex),
      kCRTCRegisterHorizontalTotal);

  // Data register. Horizontal Total defaults to 0x61.
  EXPECT_EQ(VideoReadPort(&video_, kMDAPortRegisterData), 0x61);
  VideoWritePort(&video_, kMDAPortRegisterData, 0x62);
  EXPECT_EQ(video_.registers[kCRTCRegisterHorizontalTotal], 0x62);
  EXPECT_EQ(VideoReadPort(&video_, kMDAPortRegisterData), 0x62);

  // Mode control register.
  VideoWritePort(&video_, kMDAPortControl, 0xAB);
  EXPECT_EQ(video_.control_register, 0xAB);
  EXPECT_EQ(VideoReadPort(&video_, kMDAPortControl), 0xAB);
}

TEST_F(MDATest, StatusPortIsReadOnly) {
  uint8_t status = VideoReadPort(&video_, kMDAPortStatus);
  VideoWritePort(&video_, kMDAPortStatus, 0xCD);
  EXPECT_EQ(VideoReadPort(&video_, kMDAPortStatus), status);
}

TEST_F(MDATest, PrinterPortsAreNotDecoded) {
  EXPECT_EQ(VideoReadPort(&video_, kMDAPortPrinterData), 0xFF);
  EXPECT_EQ(VideoReadPort(&video_, kMDAPortPrinterStatus), 0xFF);
  EXPECT_EQ(VideoReadPort(&video_, kMDAPortPrinterControl), 0xFF);
}

TEST_F(MDATest, VRAMAccess) {
  VideoWriteVRAM(&video_, 0x100, 0x55);
  EXPECT_EQ(mock_vram[0x100], 0x55);
  EXPECT_EQ(VideoReadVRAM(&video_, 0x100), 0x55);

  VideoWriteVRAM(&video_, 0x200, 0xAA);
  EXPECT_EQ(mock_vram[0x200], 0xAA);
  EXPECT_EQ(VideoReadVRAM(&video_, 0x200), 0xAA);

  // Out of range accesses are ignored.
  VideoWriteVRAM(&video_, kMDAVRAMSize, 0x11);
  EXPECT_EQ(mock_vram[kMDAVRAMSize], 0);
  EXPECT_EQ(VideoReadVRAM(&video_, kMDAVRAMSize), 0xFF);
}

TEST_F(MDATest, RenderCharacterNormal) {
  // 'A' with the normal attribute.
  WriteChar(0, 'A', 0x07);
  Render();

  int foreground_pixels = CountPixelsInFirstCell(config_.foreground);
  int background_pixels = CountPixelsInFirstCell(config_.background);
  EXPECT_GT(foreground_pixels, 0);
  EXPECT_GT(background_pixels, 0);
  EXPECT_EQ(foreground_pixels + background_pixels, kCharWidth * kCharHeight);
}

TEST_F(MDATest, CharacterWriteRendersTheCoveringFourLineGroups) {
  Render();

  const uint8_t kCol = 2;
  const uint8_t kRow = 1;
  WriteChar(kRow * 80 + kCol, 'A', 0x07);
  Render();

  ASSERT_EQ(mock_region_count, 1);
  EXPECT_EQ(mock_regions[0].origin.x, kCol * kCharWidth);
  // A 14-line cell beginning at line 14 covers groups 12-15 through 24-27.
  EXPECT_EQ(mock_regions[0].origin.y, 12);
  EXPECT_EQ(mock_regions[0].width, kCharWidth);
  EXPECT_EQ(mock_regions[0].height, 16);
  EXPECT_EQ(mock_pixel_write_count, kCharWidth * 16);
}

TEST_F(MDATest, RenderCharacterInverse) {
  // A space with the inverse attribute is a solid block: a space has no bits
  // set in the font, so every pixel takes the cell's background color, which
  // inverse video swaps with the foreground.
  WriteChar(0, ' ', 0x70);
  Render();

  EXPECT_EQ(
      CountPixelsInFirstCell(config_.foreground), kCharWidth * kCharHeight);
}

TEST_F(MDATest, RenderCharacterUnderline) {
  WriteChar(0, ' ', 0x01);
  Render();

  EXPECT_EQ(
      CountPixels(0, kUnderlinePosition, kCharWidth, 1, config_.foreground),
      kCharWidth);
}

TEST_F(MDATest, RenderCharacterInvisible) {
  WriteChar(0, 'A', 0x00);
  Render();

  EXPECT_EQ(
      CountPixelsInFirstCell(config_.background), kCharWidth * kCharHeight);
}

TEST_F(MDATest, RenderCharacterIntense) {
  WriteChar(0, 'A', 0x0F);
  Render();

  EXPECT_GT(CountPixelsInFirstCell(config_.intense_foreground), 0);
}

TEST_F(MDATest, RenderCharacterIntenseUnderline) {
  WriteChar(0, ' ', 0x09);
  Render();

  EXPECT_EQ(
      CountPixels(
          0, kUnderlinePosition, kCharWidth, 1, config_.intense_foreground),
      kCharWidth);
}

TEST_F(MDATest, RenderCharacterFallback) {
  // An undefined attribute combination is treated as normal.
  WriteChar(0, 'A', 0x02);
  Render();

  EXPECT_GT(CountPixelsInFirstCell(config_.foreground), 0);
}

TEST_F(MDATest, RenderBlinkingCharacter) {
  // Blink is enabled in the default mode control register.
  WriteChar(0, 'A', 0x87);
  Render();
  int visible_pixels = CountPixelsInFirstCell(config_.foreground);
  EXPECT_GT(visible_pixels, 0);

  // Advance to the opposite blink phase - the character disappears.
  AdvanceFrames(kVideoFramesPerTextBlinkPhase);
  Render();
  EXPECT_EQ(
      CountPixelsInFirstCell(config_.background), kCharWidth * kCharHeight);

  // And comes back in the phase after that.
  AdvanceFrames(kVideoFramesPerTextBlinkPhase);
  Render();
  EXPECT_EQ(CountPixelsInFirstCell(config_.foreground), visible_pixels);
}

// The 6845 generates the cursor blink itself, while the character blink
// attribute is decoded by the adapter from a separate divider running at half
// the rate. The cursor therefore toggles twice for every one time a blinking
// character does.
TEST_F(MDATest, CharactersBlinkAtHalfTheCursorRate) {
  // Put a full-height cursor on the first cell and a blinking character on the
  // second, so the two can be counted independently.
  WriteRegister(kCRTCRegisterCursorStart, 0x00);
  WriteRegister(kCRTCRegisterCursorEnd, kCharHeight - 1);
  WriteRegister(kCRTCRegisterCursorH, 0);
  WriteRegister(kCRTCRegisterCursorL, 0);
  WriteChar(1, 'A', 0x87);

  auto cursor_pixels = [&] {
    return CountPixels(0, 0, kCharWidth, kCharHeight, config_.foreground);
  };
  auto char_pixels = [&] {
    return CountPixels(
        kCharWidth, 0, kCharWidth, kCharHeight, config_.foreground);
  };

  Render();
  EXPECT_EQ(cursor_pixels(), kCharWidth * kCharHeight);
  const int visible_pixels = char_pixels();
  EXPECT_GT(visible_pixels, 0);

  // After one cursor phase the cursor has gone, but the character has not: it
  // is only halfway through its own phase.
  AdvanceFrames(kVideoFramesPerCursorBlinkPhase);
  Render();
  EXPECT_EQ(cursor_pixels(), 0);
  EXPECT_EQ(char_pixels(), visible_pixels);

  // A second cursor phase brings the cursor back and blanks the character.
  AdvanceFrames(kVideoFramesPerCursorBlinkPhase);
  Render();
  EXPECT_EQ(cursor_pixels(), kCharWidth * kCharHeight);
  EXPECT_EQ(char_pixels(), 0);
}

TEST_F(MDATest, BlinkIsIgnoredWhenDisabledInControlRegister) {
  VideoWritePort(
      &video_, kMDAPortControl,
      video_.control_register & ~kVideoControlEnableBlink);
  WriteChar(0, 'A', 0x87);
  Render();
  int visible_pixels = CountPixelsInFirstCell(config_.foreground);

  AdvanceFrames(kVideoFramesPerTextBlinkPhase);
  Render();
  EXPECT_EQ(CountPixelsInFirstCell(config_.foreground), visible_pixels);
}

TEST_F(MDATest, RenderCursor) {
  // The default cursor occupies scan lines 11 to 12 of the cell it is on.
  WriteRegister(kCRTCRegisterCursorStart, 0x0B);
  WriteRegister(kCRTCRegisterCursorEnd, 0x0C);
  // Put the cursor on the second cell of the second row.
  uint16_t cursor_cell = 80 + 1;
  WriteRegister(kCRTCRegisterCursorH, cursor_cell >> 8);
  WriteRegister(kCRTCRegisterCursorL, cursor_cell & 0xFF);
  Render();

  int cursor_x = kCharWidth;
  int cursor_y = kCharHeight;
  EXPECT_EQ(
      CountPixels(cursor_x, cursor_y + 11, kCharWidth, 2, config_.foreground),
      kCharWidth * 2);
  // The scan lines above the cursor are untouched by it.
  EXPECT_EQ(
      CountPixels(cursor_x, cursor_y, kCharWidth, 11, config_.background),
      kCharWidth * 11);
}

TEST_F(MDATest, CursorCanBeDisabled) {
  WriteRegister(kCRTCRegisterCursorEnd, 0x0C);
  WriteRegister(kCRTCRegisterCursorStart, 0x20);
  Render();

  EXPECT_EQ(
      CountPixelsInFirstCell(config_.background), kCharWidth * kCharHeight);
}

TEST_F(MDATest, StartAddressScrollsTheDisplay) {
  // Put a character on the second row of VRAM, then scroll the display up by
  // one row so that it appears in the top left corner.
  WriteChar(80, 'A', 0x07);
  Render();
  EXPECT_EQ(
      CountPixelsInFirstCell(config_.background), kCharWidth * kCharHeight);

  WriteRegister(kCRTCRegisterStartAddressH, 80 >> 8);
  WriteRegister(kCRTCRegisterStartAddressL, 80 & 0xFF);
  Render();
  EXPECT_GT(CountPixelsInFirstCell(config_.foreground), 0);
}

TEST_F(MDATest, VideoEnableBlanksTheDisplay) {
  WriteChar(0, 'A', 0x07);
  VideoWritePort(
      &video_, kMDAPortControl,
      video_.control_register & ~kVideoControlVideoEnable);
  Render();

  EXPECT_EQ(
      CountPixelsInFirstCell(config_.background), kCharWidth * kCharHeight);
  // The whole frame buffer is blanked, not just the character cells.
  EXPECT_EQ(mock_pixel_write_count, 720 * 350);
}

}  // namespace
