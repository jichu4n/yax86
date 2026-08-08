#include <gtest/gtest.h>
#include <vector>

#include "video.h"

namespace {

// Mock VRAM
static uint8_t mock_vram[kCGAVRAMSize];

static uint8_t MockReadVRAMByte(CGAState* cga, uint32_t address) {
  if (address < kCGAVRAMSize) {
    return mock_vram[address];
  }
  return 0xFF;
}

static void MockWriteVRAMByte(CGAState* cga, uint32_t address, uint8_t value) {
  if (address < kCGAVRAMSize) {
    mock_vram[address] = value;
  }
}

// Pixel recording
struct RecordedPixel {
  Position position;
  RGB rgb;
};

static std::vector<RecordedPixel> recorded_pixels;

static void MockWritePixel(CGAState* cga, Position position, RGB rgb) {
  recorded_pixels.push_back({position, rgb});
}

class CGATest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Reset mock VRAM
    for (int i = 0; i < kCGAVRAMSize; ++i) {
      mock_vram[i] = 0;
    }
    recorded_pixels.clear();

    config_.context = nullptr;
    config_.read_vram_byte = MockReadVRAMByte;
    config_.write_vram_byte = MockWriteVRAMByte;
    config_.write_pixel = MockWritePixel;

    CGAInit(&cga_, &config_);
  }

  CGAConfig config_ = {0};
  CGAState cga_ = {0};
};

TEST_F(CGATest, Initialization) {
  EXPECT_EQ(cga_.registers[0], 0x71);
  EXPECT_EQ(cga_.registers[1], 0x50);
  EXPECT_EQ(cga_.registers[2], 0x5A);
  EXPECT_EQ(cga_.registers[3], 0x0A);
  EXPECT_EQ(cga_.registers[4], 0x1F);
  EXPECT_EQ(cga_.registers[5], 0x06);
  EXPECT_EQ(cga_.registers[6], 0x19);
  EXPECT_EQ(cga_.registers[7], 0x1C);
  EXPECT_EQ(cga_.registers[8], 0x02);
  EXPECT_EQ(cga_.registers[9], 0x07);
  EXPECT_EQ(cga_.registers[10], 0x06);
  EXPECT_EQ(cga_.registers[11], 0x07);
  
  EXPECT_EQ(cga_.mode_control, 0x29);
  EXPECT_EQ(cga_.color_select, 0x00);
  
  // Verify VRAM was cleared (initialized to space ' ' and attribute 0x07)
  EXPECT_EQ(mock_vram[0], ' ');
  EXPECT_EQ(mock_vram[1], 0x07);
  EXPECT_EQ(mock_vram[kCGAVRAMSize - 2], ' ');
  EXPECT_EQ(mock_vram[kCGAVRAMSize - 1], 0x07);
}

TEST_F(CGATest, PortReadWrite) {
  // Index Register
  CGAWritePort(&cga_, kCGAPortRegisterIndex, 1);
  EXPECT_EQ(cga_.selected_register, 1);
  EXPECT_EQ(CGAReadPort(&cga_, kCGAPortRegisterIndex), 1);

  // Data Register (write to selected register)
  EXPECT_EQ(CGAReadPort(&cga_, kCGAPortRegisterData), 0x50); // Default of R1
  CGAWritePort(&cga_, kCGAPortRegisterData, 0x55);
  EXPECT_EQ(cga_.registers[1], 0x55);
  EXPECT_EQ(CGAReadPort(&cga_, kCGAPortRegisterData), 0x55);

  // Mode Control Port
  CGAWritePort(&cga_, kCGAPortModeControl, 0x0A);
  EXPECT_EQ(cga_.mode_control, 0x0A);
  // Control port is write-only, reading it might not return the value in real hardware,
  // but according to current implementation it returns 0xFF for unhandled ports.
  EXPECT_EQ(CGAReadPort(&cga_, kCGAPortModeControl), 0xFF);

  // Color Select Port
  CGAWritePort(&cga_, kCGAPortColorSelect, 0x1F);
  EXPECT_EQ(cga_.color_select, 0x1F);
}

TEST_F(CGATest, StatusRegisterToggling) {
  uint8_t initial_status = cga_.status;
  
  uint8_t read1 = CGAReadPort(&cga_, kCGAPortStatus);
  EXPECT_EQ(read1, initial_status ^ (kCGAStatusDisplayEnable | kCGAStatusVSync));
  
  uint8_t read2 = CGAReadPort(&cga_, kCGAPortStatus);
  EXPECT_EQ(read2, initial_status);
  
  uint8_t read3 = CGAReadPort(&cga_, kCGAPortStatus);
  EXPECT_EQ(read3, initial_status ^ (kCGAStatusDisplayEnable | kCGAStatusVSync));
}

TEST_F(CGATest, VRAMAccess) {
  CGAWriteVRAM(&cga_, 0x100, 0x55);
  EXPECT_EQ(mock_vram[0x100], 0x55);
  EXPECT_EQ(CGAReadVRAM(&cga_, 0x100), 0x55);

  CGAWriteVRAM(&cga_, 0x200, 0xAA);
  EXPECT_EQ(mock_vram[0x200], 0xAA);
  EXPECT_EQ(CGAReadVRAM(&cga_, 0x200), 0xAA);
}

TEST_F(CGATest, GetCurrentModeMetadata) {
  cga_.mode_control = 0x29; // 80-col, video, blink -> Mode 3
  EXPECT_EQ(CGAGetCurrentModeMetadata(&cga_)->mode, kCGAText03);

  cga_.mode_control = 0x2D; // 80-col, BW, video, blink -> Mode 2
  EXPECT_EQ(CGAGetCurrentModeMetadata(&cga_)->mode, kCGAText02);

  cga_.mode_control = 0x28; // 40-col, video, blink -> Mode 1
  EXPECT_EQ(CGAGetCurrentModeMetadata(&cga_)->mode, kCGAText01);

  cga_.mode_control = 0x2C; // 40-col, BW, video, blink -> Mode 0
  EXPECT_EQ(CGAGetCurrentModeMetadata(&cga_)->mode, kCGAText00);

  cga_.mode_control = 0x0A; // graphics, video -> Mode 4
  EXPECT_EQ(CGAGetCurrentModeMetadata(&cga_)->mode, kCGAGraphics04);

  cga_.mode_control = 0x0E; // graphics, BW, video -> Mode 5
  EXPECT_EQ(CGAGetCurrentModeMetadata(&cga_)->mode, kCGAGraphics05);

  cga_.mode_control = 0x1A; // graphics, hires, video -> Mode 6
  EXPECT_EQ(CGAGetCurrentModeMetadata(&cga_)->mode, kCGAGraphics06);
}

// Define palette for verification in tests
static const RGB kCGAPalette[16] = {
    {.r = 0x00, .g = 0x00, .b = 0x00},
    {.r = 0x00, .g = 0x00, .b = 0xAA},
    {.r = 0x00, .g = 0xAA, .b = 0x00},
    {.r = 0x00, .g = 0xAA, .b = 0xAA},
    {.r = 0xAA, .g = 0x00, .b = 0x00},
    {.r = 0xAA, .g = 0x00, .b = 0xAA},
    {.r = 0xAA, .g = 0x55, .b = 0x00},
    {.r = 0xAA, .g = 0xAA, .b = 0xAA},
    {.r = 0x55, .g = 0x55, .b = 0x55},
    {.r = 0x55, .g = 0x55, .b = 0xFF},
    {.r = 0x55, .g = 0xFF, .b = 0x55},
    {.r = 0x55, .g = 0xFF, .b = 0xFF},
    {.r = 0xFF, .g = 0x55, .b = 0x55},
    {.r = 0xFF, .g = 0x55, .b = 0xFF},
    {.r = 0xFF, .g = 0xFF, .b = 0x55},
    {.r = 0xFF, .g = 0xFF, .b = 0xFF},
};

static bool RGBEq(RGB a, RGB b) {
  return a.r == b.r && a.g == b.g && a.b == b.b;
}

TEST_F(CGATest, RenderTextNormal) {
  // Mode 3 (80x25 text)
  cga_.mode_control = 0x29;
  
  // Write 'A' (0x41) with attribute 0x07 (light gray on black) at (0,0)
  CGAWriteVRAM(&cga_, 0, 'A');
  CGAWriteVRAM(&cga_, 1, 0x07);

  CGARender(&cga_);

  int foreground_pixel_count = 0;
  int background_pixel_count = 0;

  for (const auto& pixel : recorded_pixels) {
    if (pixel.position.x < 8 && pixel.position.y < 8) {
      if (RGBEq(pixel.rgb, kCGAPalette[7])) {
        foreground_pixel_count++;
      } else if (RGBEq(pixel.rgb, kCGAPalette[0])) {
        background_pixel_count++;
      }
    }
  }

  EXPECT_GT(foreground_pixel_count, 0);
  EXPECT_GT(background_pixel_count, 0);
  EXPECT_EQ(foreground_pixel_count + background_pixel_count, 8 * 8);
}

TEST_F(CGATest, RenderTextColor) {
  cga_.mode_control = 0x29;
  
  // Write 'A' (0x41) with attribute 0x1E (yellow on blue) at (0,0)
  CGAWriteVRAM(&cga_, 0, 'A');
  CGAWriteVRAM(&cga_, 1, 0x1E);

  CGARender(&cga_);

  int foreground_pixel_count = 0;
  int background_pixel_count = 0;

  for (const auto& pixel : recorded_pixels) {
    if (pixel.position.x < 8 && pixel.position.y < 8) {
      if (RGBEq(pixel.rgb, kCGAPalette[14])) {
        foreground_pixel_count++;
      } else if (RGBEq(pixel.rgb, kCGAPalette[1])) {
        background_pixel_count++;
      }
    }
  }

  EXPECT_GT(foreground_pixel_count, 0);
  EXPECT_GT(background_pixel_count, 0);
  EXPECT_EQ(foreground_pixel_count + background_pixel_count, 8 * 8);
}

TEST_F(CGATest, RenderText40Column) {
  cga_.mode_control = 0x28; // Mode 1
  
  const VideoModeMetadata* metadata = CGAGetCurrentModeMetadata(&cga_);
  EXPECT_EQ(metadata->columns, 40);
  EXPECT_EQ(metadata->width, 320);

  CGAWriteVRAM(&cga_, 0, 'A');
  CGAWriteVRAM(&cga_, 1, 0x07);

  CGARender(&cga_);
  
  // Just verify some pixels were drawn.
  EXPECT_GT(recorded_pixels.size(), 0);
}

TEST_F(CGATest, RenderGraphics320) {
  cga_.mode_control = 0x0A; // Mode 4 (320x200 graphics)
  // Palette selection (let's use default color_select = 0x00, Palette 1, bg = black)
  // byte_value = 0b11100100 (0xE4)
  // pixels: 11 (color 3), 10 (color 2), 01 (color 1), 00 (color 0)
  CGAWriteVRAM(&cga_, 0, 0xE4);
  
  CGARender(&cga_);

  // Check the first 4 pixels on row 0
  // Default palette 1 (no intensity): colors are Cyan, Magenta, White
  RGB color3 = kCGAPalette[7];
  RGB color2 = kCGAPalette[5];
  RGB color1 = kCGAPalette[3];
  RGB color0 = kCGAPalette[0];
  
  bool found_0 = false, found_1 = false, found_2 = false, found_3 = false;
  
  for (const auto& pixel : recorded_pixels) {
    if (pixel.position.y == 0) {
      if (pixel.position.x == 0 && RGBEq(pixel.rgb, color3)) found_0 = true;
      if (pixel.position.x == 1 && RGBEq(pixel.rgb, color2)) found_1 = true;
      if (pixel.position.x == 2 && RGBEq(pixel.rgb, color1)) found_2 = true;
      if (pixel.position.x == 3 && RGBEq(pixel.rgb, color0)) found_3 = true;
    }
  }

  EXPECT_TRUE(found_0);
  EXPECT_TRUE(found_1);
  EXPECT_TRUE(found_2);
  EXPECT_TRUE(found_3);
}

TEST_F(CGATest, RenderGraphics320OddScanline) {
  cga_.mode_control = 0x0A; // Mode 4
  
  // Write to bank 1, offset 0 -> corresponds to y=1, x=0..3
  CGAWriteVRAM(&cga_, 0x2000, 0xE4);
  
  CGARender(&cga_);

  RGB color3 = kCGAPalette[7];
  
  bool found_y1_x0 = false;
  
  for (const auto& pixel : recorded_pixels) {
    if (pixel.position.y == 1 && pixel.position.x == 0 && RGBEq(pixel.rgb, color3)) {
      found_y1_x0 = true;
    }
  }

  EXPECT_TRUE(found_y1_x0);
}

TEST_F(CGATest, RenderGraphics640) {
  cga_.mode_control = 0x1A; // Mode 6 (640x200 graphics, hires)
  cga_.color_select = 0x0F; // Foreground color 15 (white)
  
  // byte_value = 0b10101010 (0xAA)
  CGAWriteVRAM(&cga_, 0, 0xAA);
  
  CGARender(&cga_);

  RGB fg = kCGAPalette[15];
  RGB bg = kCGAPalette[0];
  
  int matched = 0;
  for (const auto& pixel : recorded_pixels) {
    if (pixel.position.y == 0 && pixel.position.x < 8) {
      bool expected_is_set = (0xAA >> (7 - pixel.position.x)) & 1;
      RGB expected_color = expected_is_set ? fg : bg;
      if (RGBEq(pixel.rgb, expected_color)) {
        matched++;
      }
    }
  }

  EXPECT_EQ(matched, 8);
}

TEST_F(CGATest, VideoDisableNoRender) {
  cga_.mode_control = 0x29 & ~kCGAModeControlVideoEnable; // Disable video
  
  CGARender(&cga_);

  EXPECT_EQ(recorded_pixels.size(), 0);
}

}  // namespace
