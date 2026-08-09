#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "video.h"

namespace {

enum {
  kFrameBufferWidth = 720,
  kFrameBufferHeight = 350,
  kCharWidth = 9,
  kCharHeight = 14,
  // Scan line within a character cell that the underline occupies.
  kUnderlinePosition = 12,
};

// Mock VRAM.
static uint8_t mock_vram[kMDAVRAMSize];

static uint8_t MockReadVRAMByte(VideoState* video, uint32_t address) {
  if (address < kMDAVRAMSize) {
    return mock_vram[address];
  }
  return 0xFF;
}

static void MockWriteVRAMByte(
    VideoState* video, uint32_t address, uint8_t value) {
  if (address < kMDAVRAMSize) {
    mock_vram[address] = value;
  }
}

// Mock frame buffer. Rendering writes into it the same way a real display
// would, so a test can simply ask what color a pixel ended up.
static RGB mock_frame_buffer[kFrameBufferHeight][kFrameBufferWidth];

static void MockWritePixel(VideoState* video, Position position, RGB rgb) {
  if (position.x < kFrameBufferWidth && position.y < kFrameBufferHeight) {
    mock_frame_buffer[position.y][position.x] = rgb;
  }
}

static bool operator==(const RGB& a, const RGB& b) {
  return a.r == b.r && a.g == b.g && a.b == b.b;
}

// Prints an RGB value on assertion failure rather than dumping its bytes.
static void PrintTo(const RGB& rgb, std::ostream* os) {
  *os << "RGB(" << static_cast<int>(rgb.r) << ", " << static_cast<int>(rgb.g)
      << ", " << static_cast<int>(rgb.b) << ")";
}

class MDATest : public ::testing::Test {
 protected:
  void SetUp() override {
    for (int i = 0; i < kMDAVRAMSize; ++i) {
      mock_vram[i] = 0;
    }
    ClearFrameBuffer();

    config_ = kDefaultVideoConfig;
    config_.read_vram_byte = MockReadVRAMByte;
    config_.write_vram_byte = MockWriteVRAMByte;
    config_.write_pixel = MockWritePixel;

    VideoInit(&video_, &config_);
  }

  void ClearFrameBuffer() {
    static const RGB kNoPixel = {.r = 0xDE, .g = 0xAD, .b = 0xBE};
    for (int y = 0; y < kFrameBufferHeight; ++y) {
      for (int x = 0; x < kFrameBufferWidth; ++x) {
        mock_frame_buffer[y][x] = kNoPixel;
      }
    }
  }

  // Render a frame into a freshly cleared frame buffer.
  void Render() {
    ClearFrameBuffer();
    VideoRender(&video_);
  }

  // Number of pixels of the given color within a rectangle.
  int CountPixels(int x, int y, int width, int height, RGB rgb) const {
    int count = 0;
    for (int row = y; row < y + height; ++row) {
      for (int col = x; col < x + width; ++col) {
        if (mock_frame_buffer[row][col] == rgb) {
          ++count;
        }
      }
    }
    return count;
  }

  // Number of pixels of the given color within the first character cell.
  int CountPixelsInFirstCell(RGB rgb) const {
    return CountPixels(0, 0, kCharWidth, kCharHeight, rgb);
  }

  // Write a character and its attribute at a character cell offset.
  void WriteChar(uint32_t cell, uint8_t character, uint8_t attribute) {
    mock_vram[cell * 2] = character;
    mock_vram[cell * 2 + 1] = attribute;
  }

  VideoConfig config_ = {0};
  VideoState video_ = {0};
};

TEST_F(MDATest, Initialization) {
  EXPECT_EQ(video_.control_register, 0x29);
  EXPECT_EQ(video_.selected_register, 0);
  EXPECT_EQ(video_.registers[kCRTCRegisterHorizontalTotal], 0x61);
  // Verify VRAM was cleared to spaces with the default attribute.
  EXPECT_EQ(mock_vram[0], ' ');
  EXPECT_EQ(mock_vram[1], 0x07);
  EXPECT_EQ(mock_vram[kMDAVRAMSize - 2], ' ');
  EXPECT_EQ(mock_vram[kMDAVRAMSize - 1], 0x07);
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

TEST_F(MDATest, RegisterIndexIsMaskedToFiveBits) {
  VideoWritePort(&video_, kMDAPortRegisterIndex, 0xE1);
  EXPECT_EQ(video_.selected_register, 0x01);
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

// ============================================================================
// Retrace timing
// ============================================================================

TEST_F(MDATest, StatusStartsInActiveDisplay) {
  uint8_t status = VideoReadPort(&video_, kMDAPortStatus);
  EXPECT_EQ(status & kVideoStatusDisplayDisabled, 0);
  EXPECT_EQ(status & kVideoStatusVerticalRetrace, 0);
  // No light pen is emulated, so its switch always reads as off.
  EXPECT_NE(status & kVideoStatusLightPenSwitchOff, 0);
  EXPECT_EQ(status & kVideoStatusLightPenTrigger, 0);
}

TEST_F(MDATest, HorizontalRetraceWithinAScanLine) {
  // Partway through the active part of a scan line the display is still on.
  VideoTick(&video_, kMDADisplayCyclesPerScanLine - 1);
  EXPECT_EQ(
      VideoReadPort(&video_, kMDAPortStatus) & kVideoStatusDisplayDisabled, 0);

  // Past the end of it, the display is disabled but there is no vertical
  // retrace yet.
  VideoTick(&video_, 1);
  EXPECT_NE(
      VideoReadPort(&video_, kMDAPortStatus) & kVideoStatusDisplayDisabled, 0);
  EXPECT_EQ(
      VideoReadPort(&video_, kMDAPortStatus) & kVideoStatusVerticalRetrace, 0);

  // The next scan line starts with the display back on.
  VideoTick(&video_, kMDACyclesPerScanLine - kMDADisplayCyclesPerScanLine);
  EXPECT_EQ(
      VideoReadPort(&video_, kMDAPortStatus) & kVideoStatusDisplayDisabled, 0);
  EXPECT_EQ(video_.scan_line, 1);
}

TEST_F(MDATest, VerticalRetraceAtTheEndOfAFrame) {
  // Run to the last displayed scan line.
  for (uint16_t i = 0; i < kMDADisplayedScanLines - 1; ++i) {
    VideoTick(&video_, kMDACyclesPerScanLine);
  }
  EXPECT_EQ(
      VideoReadPort(&video_, kMDAPortStatus) & kVideoStatusVerticalRetrace, 0);

  // One scan line later the vertical retrace begins, and stays set for the
  // rest of the frame.
  VideoTick(&video_, kMDACyclesPerScanLine);
  EXPECT_NE(
      VideoReadPort(&video_, kMDAPortStatus) & kVideoStatusVerticalRetrace, 0);
  EXPECT_EQ(video_.frames, 0u);

  for (uint16_t i = 0; i < kMDAScanLinesPerFrame - kMDADisplayedScanLines - 1;
       ++i) {
    VideoTick(&video_, kMDACyclesPerScanLine);
  }
  EXPECT_NE(
      VideoReadPort(&video_, kMDAPortStatus) & kVideoStatusVerticalRetrace, 0);

  // And clears at the start of the next frame.
  VideoTick(&video_, kMDACyclesPerScanLine);
  EXPECT_EQ(
      VideoReadPort(&video_, kMDAPortStatus) & kVideoStatusVerticalRetrace, 0);
  EXPECT_EQ(video_.scan_line, 0);
  EXPECT_EQ(video_.frames, 1u);
}

TEST_F(MDATest, TickAccumulatesPartialScanLines) {
  // A cycle count that does not divide the scan line period still advances the
  // beam at the right average rate.
  const uint16_t kCyclesPerTick = 17;
  uint32_t total_cycles = 0;
  for (int i = 0; i < 100; ++i) {
    VideoTick(&video_, kCyclesPerTick);
    total_cycles += kCyclesPerTick;
  }
  EXPECT_EQ(video_.scan_line, total_cycles / kMDACyclesPerScanLine);
  EXPECT_EQ(video_.scan_line_cycles, total_cycles % kMDACyclesPerScanLine);
}

TEST_F(MDATest, StatusHighBitsStayClear) {
  // Bit 7 in particular: the BIOS probes it on the MDA status port to detect a
  // Hercules adapter, which this is not.
  for (uint16_t i = 0; i < kMDAScanLinesPerFrame; ++i) {
    EXPECT_EQ(VideoReadPort(&video_, kMDAPortStatus) & 0xF0, 0);
    VideoTick(&video_, kMDACyclesPerScanLine);
  }
}

TEST_F(MDATest, UnselectedRegisterReadsAsUnmapped) {
  // The 6845 has 18 registers, and the index masks to five bits, so indices 18
  // to 31 select nothing.
  VideoWritePort(&video_, kMDAPortRegisterIndex, kNumCRTCRegisters);
  EXPECT_EQ(VideoReadPort(&video_, kMDAPortRegisterData), 0xFF);
  VideoWritePort(&video_, kMDAPortRegisterData, 0x55);
  EXPECT_EQ(VideoReadPort(&video_, kMDAPortRegisterData), 0xFF);
}

TEST_F(MDATest, RenderIsSafeWithoutCallbacks) {
  // A host that has not installed its callbacks yet must not crash the
  // renderer. The platform initializes the video module before the host has a
  // chance to install them.
  VideoConfig bare_config = kDefaultVideoConfig;
  VideoState bare_video = {0};
  VideoInit(&bare_video, &bare_config);
  VideoRender(&bare_video);
  VideoTick(&bare_video, 1000);
  EXPECT_EQ(VideoReadVRAM(&bare_video, 0), 0xFF);
}

}  // namespace
