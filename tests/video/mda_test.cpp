#include <gtest/gtest.h>
#include <vector>

#include "video.h"

namespace {

// Mock VRAM
static uint8_t mock_vram[kMDAVRAMSize];

static uint8_t MockReadVRAMByte(MDAState* mda, uint32_t address) {
  if (address < kMDAVRAMSize) {
    return mock_vram[address];
  }
  return 0xFF;
}

static void MockWriteVRAMByte(MDAState* mda, uint32_t address, uint8_t value) {
  if (address < kMDAVRAMSize) {
    mock_vram[address] = value;
  }
}

// Pixel recording
struct RecordedPixel {
  Position position;
  RGB rgb;
};

static std::vector<RecordedPixel> recorded_pixels;

static void MockWritePixel(MDAState* mda, Position position, RGB rgb) {
  recorded_pixels.push_back({position, rgb});
}

class MDATest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Reset mock VRAM
    for (int i = 0; i < kMDAVRAMSize; ++i) {
      mock_vram[i] = 0;
    }
    recorded_pixels.clear();

    config_.context = nullptr;
    config_.read_vram_byte = MockReadVRAMByte;
    config_.write_vram_byte = MockWriteVRAMByte;
    config_.write_pixel = MockWritePixel;
    // Set default colors
    config_.foreground = {.r = 0xAA, .g = 0xAA, .b = 0xAA};
    config_.intense_foreground = {.r = 0xFF, .g = 0xFF, .b = 0xFF};
    config_.background = {.r = 0x00, .g = 0x00, .b = 0x00};

    MDAInit(&mda_, &config_);
  }

  MDAConfig config_ = {0};
  MDAState mda_ = {0};
};

TEST_F(MDATest, Initialization) {
  EXPECT_EQ(mda_.control_port, 0x29);
  EXPECT_EQ(mda_.selected_register, 0);
  // Verify VRAM was cleared (initialized to space ' ' and attribute 0x07)
  // MDAInit loops over VRAM size.
  // Checking a few bytes is enough.
  EXPECT_EQ(mock_vram[0], ' ');
  EXPECT_EQ(mock_vram[1], 0x07);
  EXPECT_EQ(mock_vram[kMDAVRAMSize - 2], ' ');
  EXPECT_EQ(mock_vram[kMDAVRAMSize - 1], 0x07);
}

TEST_F(MDATest, PortReadWrite) {
  // Index Register
  MDAWritePort(&mda_, kMDAPortRegisterIndex, kMDARegisterHorizontalTotal);
  EXPECT_EQ(mda_.selected_register, kMDARegisterHorizontalTotal);
  EXPECT_EQ(MDAReadPort(&mda_, kMDAPortRegisterIndex), kMDARegisterHorizontalTotal);

  // Data Register (write to selected register)
  // Horizontal Total default is 0x61
  EXPECT_EQ(MDAReadPort(&mda_, kMDAPortRegisterData), 0x61);
  MDAWritePort(&mda_, kMDAPortRegisterData, 0x62);
  EXPECT_EQ(mda_.registers[kMDARegisterHorizontalTotal], 0x62);
  EXPECT_EQ(MDAReadPort(&mda_, kMDAPortRegisterData), 0x62);

  // Control Port
  MDAWritePort(&mda_, kMDAPortControl, 0xAB);
  EXPECT_EQ(mda_.control_port, 0xAB);
  EXPECT_EQ(MDAReadPort(&mda_, kMDAPortControl), 0xAB);

  // Status Port
  MDAWritePort(&mda_, kMDAPortStatus, 0xCD);
  EXPECT_EQ(mda_.status_port, 0xCD);
  EXPECT_EQ(MDAReadPort(&mda_, kMDAPortStatus), 0xCD);
}

TEST_F(MDATest, VRAMAccess) {
  MDAWriteVRAM(&mda_, 0x100, 0x55);
  EXPECT_EQ(mock_vram[0x100], 0x55);
  EXPECT_EQ(MDAReadVRAM(&mda_, 0x100), 0x55);

  MDAWriteVRAM(&mda_, 0x200, 0xAA);
  EXPECT_EQ(mock_vram[0x200], 0xAA);
  EXPECT_EQ(MDAReadVRAM(&mda_, 0x200), 0xAA);
}

TEST_F(MDATest, RenderCharacterNormal) {
  // Write 'A' (0x41) with Normal attribute (0x07) at (0,0)
  MDAWriteVRAM(&mda_, 0, 'A');
  MDAWriteVRAM(&mda_, 1, 0x07);

  // Render
  MDARender(&mda_);

  // 'A' is 9x14 pixels.
  // We expect calls to write_pixel.
  // Since we render the whole screen, there will be MANY calls.
  // However, we only care about the first char (top-left).
  // The first 9x14 pixels correspond to 'A'.
  
  // Let's verify some pixels of 'A'.
  // We need access to the font bitmap to know what to expect.
  // But we can just check if *any* foreground pixels were written.
  
  // We can't easily access kFontMDA9x14Bitmap from here unless we duplicate it or it's public.
  // It's likely not public.
  // However, we know 'A' has some pixels.
  
  int foreground_pixel_count = 0;
  int background_pixel_count = 0;

  // Check the first 9x14 pixels (char at 0,0)
  for (const auto& pixel : recorded_pixels) {
    if (pixel.position.x < 9 && pixel.position.y < 14) {
      if (pixel.rgb.r == config_.foreground.r &&
          pixel.rgb.g == config_.foreground.g &&
          pixel.rgb.b == config_.foreground.b) {
        foreground_pixel_count++;
      } else if (pixel.rgb.r == config_.background.r &&
                 pixel.rgb.g == config_.background.g &&
                 pixel.rgb.b == config_.background.b) {
        background_pixel_count++;
      }
    }
  }

  // 'A' should have some foreground and some background pixels.
  EXPECT_GT(foreground_pixel_count, 0);
  EXPECT_GT(background_pixel_count, 0);
  // Total pixels for one char
  EXPECT_EQ(foreground_pixel_count + background_pixel_count, 9 * 14);
}

TEST_F(MDATest, RenderCharacterInverse) {
  // Write ' ' (0x20) with Inverse attribute (0x70: bg=111, fg=000)
  MDAWriteVRAM(&mda_, 0, ' '); // Space is usually empty
  MDAWriteVRAM(&mda_, 1, 0x70);

  MDARender(&mda_);

  // Space in inverse mode should be a solid block of the "foreground" color 
  // (which is actually the config.background color swapped).
  // Wait, logic in MDAWriteChar:
  // if (background_attr == 0x07 && foreground_attr == 0x00) {
  //   foreground = &mda->config->background;
  //   background = &mda->config->foreground;
  // }
  // A space ' ' has 0 bits set in the bitmap.
  // So all pixels will use the 'background' pointer.
  // In inverse mode, 'background' pointer points to config.foreground.
  
  int inverse_background_pixels = 0;
   for (const auto& pixel : recorded_pixels) {
    if (pixel.position.x < 9 && pixel.position.y < 14) {
      if (pixel.rgb.r == config_.foreground.r &&
          pixel.rgb.g == config_.foreground.g &&
          pixel.rgb.b == config_.foreground.b) {
        inverse_background_pixels++;
      }
    }
  }
  
  // All 9x14 pixels should be the "foreground" color (because space is empty, showing background, which is swapped).
  EXPECT_EQ(inverse_background_pixels, 9 * 14);
}

TEST_F(MDATest, RenderCharacterUnderline) {
    // Write ' ' (0x20) with Underline attribute (0x01: bg=000, fg=001)
    MDAWriteVRAM(&mda_, 0, ' ');
    MDAWriteVRAM(&mda_, 1, 0x01);

    MDARender(&mda_);

    // Check row 12 (12th index, 0-based) for underline.
    // kMDAUnderlinePosition = 12.
    int underline_pixels = 0;
    for (const auto& pixel : recorded_pixels) {
        if (pixel.position.x < 9 && pixel.position.y == 12) {
            if (pixel.rgb.r == config_.foreground.r &&
                pixel.rgb.g == config_.foreground.g &&
                pixel.rgb.b == config_.foreground.b) {
                underline_pixels++;
            }
        }
    }
    // All 9 pixels in the underline row should be foreground.
    EXPECT_EQ(underline_pixels, 9);
}

TEST_F(MDATest, RenderCharacterInvisible) {
  // Write 'A' (0x41) with Invisible attribute (0x00: bg=000, fg=000)
  MDAWriteVRAM(&mda_, 0, 'A');
  MDAWriteVRAM(&mda_, 1, 0x00);

  MDARender(&mda_);

  int visible_pixels = 0;
  for (const auto& pixel : recorded_pixels) {
    if (pixel.position.x < 9 && pixel.position.y < 14) {
      if (pixel.rgb.r != config_.background.r ||
          pixel.rgb.g != config_.background.g ||
          pixel.rgb.b != config_.background.b) {
        visible_pixels++;
      }
    }
  }
  // All pixels should be background color (invisible).
  EXPECT_EQ(visible_pixels, 0);
}

TEST_F(MDATest, RenderCharacterIntense) {
  // Write 'A' (0x41) with Intense Normal attribute (0x0F: bg=000, intense=1, fg=111)
  MDAWriteVRAM(&mda_, 0, 'A');
  MDAWriteVRAM(&mda_, 1, 0x0F);

  MDARender(&mda_);

  int intense_pixels = 0;
  for (const auto& pixel : recorded_pixels) {
    if (pixel.position.x < 9 && pixel.position.y < 14) {
      if (pixel.rgb.r == config_.intense_foreground.r &&
          pixel.rgb.g == config_.intense_foreground.g &&
          pixel.rgb.b == config_.intense_foreground.b) {
        intense_pixels++;
      }
    }
  }
  // 'A' should have some intense foreground pixels.
  EXPECT_GT(intense_pixels, 0);
}

TEST_F(MDATest, RenderCharacterIntenseUnderline) {
  // Write ' ' (0x20) with Intense Underline attribute (0x09: bg=000, intense=1, fg=001)
  MDAWriteVRAM(&mda_, 0, ' ');
  MDAWriteVRAM(&mda_, 1, 0x09);

  MDARender(&mda_);

  // Check row 12 for intense underline.
  int intense_underline_pixels = 0;
  for (const auto& pixel : recorded_pixels) {
    if (pixel.position.x < 9 && pixel.position.y == 12) {
      if (pixel.rgb.r == config_.intense_foreground.r &&
          pixel.rgb.g == config_.intense_foreground.g &&
          pixel.rgb.b == config_.intense_foreground.b) {
        intense_underline_pixels++;
      }
    }
  }
  // All 9 pixels in the underline row should be intense foreground.
  EXPECT_EQ(intense_underline_pixels, 9);
}

TEST_F(MDATest, RenderCharacterFallback) {
  // Write 'A' (0x41) with undefined attribute (0x02: bg=000, fg=010)
  // Should be treated as Normal.
  MDAWriteVRAM(&mda_, 0, 'A');
  MDAWriteVRAM(&mda_, 1, 0x02);

  MDARender(&mda_);

  int normal_pixels = 0;
  for (const auto& pixel : recorded_pixels) {
    if (pixel.position.x < 9 && pixel.position.y < 14) {
      if (pixel.rgb.r == config_.foreground.r &&
          pixel.rgb.g == config_.foreground.g &&
          pixel.rgb.b == config_.foreground.b) {
        normal_pixels++;
      }
    }
  }
  // 'A' should have some normal foreground pixels.
  EXPECT_GT(normal_pixels, 0);
}

}  // namespace
