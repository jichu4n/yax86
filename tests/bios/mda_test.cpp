#include <gtest/gtest.h>

#include "bios.h"
#include "video_test_helper.h"

using namespace std;

class MDATest : public ::testing::Test {};

TEST_F(MDATest, RenderBlankScreen) {
  VideoTestHelper helper;
  SwitchVideoMode(&helper.bios_state_, kVideoTextModeMDA07);
  EXPECT_TRUE(helper.RenderToPPM("mda_test_blank.ppm"));
}

TEST_F(MDATest, RenderHelloWorld) {
  VideoTestHelper helper;
  SwitchVideoMode(&helper.bios_state_, kVideoTextModeMDA07);
  const VideoModeMetadata* metadata =
      GetCurrentVideoModeMetadata(&helper.bios_state_);
  const string text = "Hello, world!";
  for (size_t i = 0, address = metadata->vram_address; i < text.size();
       ++i, address += 2) {
    WriteMemoryByte(&helper.bios_state_, address, text[i]);
    WriteMemoryByte(&helper.bios_state_, address + 1, 0x07);
  }
  EXPECT_TRUE(helper.RenderToPPM("mda_test_hello.ppm"));
}

TEST_F(MDATest, RenderAllASCII) {
  VideoTestHelper helper;
  SwitchVideoMode(&helper.bios_state_, kVideoTextModeMDA07);
  const VideoModeMetadata* metadata =
      GetCurrentVideoModeMetadata(&helper.bios_state_);
  for (size_t i = 0, address = metadata->vram_address; i < 256;
       ++i, address += 2) {
    WriteMemoryByte(&helper.bios_state_, address, i);
    WriteMemoryByte(&helper.bios_state_, address + 1, 0x07);
  }
  EXPECT_TRUE(helper.RenderToPPM("mda_test_all_ascii.ppm"));
}

TEST_F(MDATest, RenderAttributes) {
  VideoTestHelper helper;
  SwitchVideoMode(&helper.bios_state_, kVideoTextModeMDA07);
  const VideoModeMetadata* metadata =
      GetCurrentVideoModeMetadata(&helper.bios_state_);
  const string text = "### Testing various character attributes!! ###";
  for (size_t i = 0, address = metadata->vram_address; i < text.size();
       ++i, address += 2) {
    WriteMemoryByte(&helper.bios_state_, address, text[i]);
    WriteMemoryByte(&helper.bios_state_, address + 1, 0x70);  // Reverse video
  }
  EXPECT_TRUE(helper.RenderToPPM("mda_test_reverse.ppm"));

  // Now underline the text.
  for (size_t i = 0, address = metadata->vram_address; i < text.size();
       ++i, address += 2) {
    WriteMemoryByte(&helper.bios_state_, address + 1, 0x01);  // Underline
  }
  EXPECT_TRUE(helper.RenderToPPM("mda_test_underline.ppm"));

  // Now set intense foreground color.
  for (size_t i = 0, address = metadata->vram_address; i < text.size();
       ++i, address += 2) {
    WriteMemoryByte(
        &helper.bios_state_, address + 1, 0x08);  // Intense foreground
  }
  EXPECT_TRUE(helper.RenderToPPM("mda_test_intense.ppm"));

  // Test intense + underline.
  for (size_t i = 0, address = metadata->vram_address; i < text.size();
       ++i, address += 2) {
    WriteMemoryByte(
        &helper.bios_state_, address + 1, 0x09);  // Intense + underline
  }
  EXPECT_TRUE(helper.RenderToPPM("mda_test_intense_underline.ppm"));
}
