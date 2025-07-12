#include <gtest/gtest.h>

#include "bios.h"
#include "bios_test_helper.h"

using namespace std;

class MDATest : public ::testing::Test {};

TEST_F(MDATest, RenderBlankScreen) {
  BIOSTestHelper helper;
  SwitchVideoMode(&helper.bios_, kVideoTextModeMDA07);
  EXPECT_TRUE(helper.RenderToFileAndCheckGolden("mda_test_blank"));
}

TEST_F(MDATest, RenderHelloWorld) {
  BIOSTestHelper helper;
  SwitchVideoMode(&helper.bios_, kVideoTextModeMDA07);
  const VideoModeMetadata* metadata =
      GetCurrentVideoModeMetadata(&helper.bios_);
  const string text = "Hello, world!";
  for (size_t i = 0, address = metadata->vram_address; i < text.size();
       ++i, address += 2) {
    WriteMemoryByte(&helper.bios_, address, text[i]);
    WriteMemoryByte(&helper.bios_, address + 1, 0x07);
  }
  EXPECT_TRUE(helper.RenderToFileAndCheckGolden("mda_test_hello"));
}

TEST_F(MDATest, RenderAllASCII) {
  BIOSTestHelper helper;
  SwitchVideoMode(&helper.bios_, kVideoTextModeMDA07);
  const VideoModeMetadata* metadata =
      GetCurrentVideoModeMetadata(&helper.bios_);
  for (size_t i = 0, address = metadata->vram_address; i < 256;
       ++i, address += 2) {
    WriteMemoryByte(&helper.bios_, address, i);
    WriteMemoryByte(&helper.bios_, address + 1, 0x07);
  }
  EXPECT_TRUE(helper.RenderToFileAndCheckGolden("mda_test_all_ascii"));
}

TEST_F(MDATest, RenderAttributes) {
  BIOSTestHelper helper;
  SwitchVideoMode(&helper.bios_, kVideoTextModeMDA07);
  const VideoModeMetadata* metadata =
      GetCurrentVideoModeMetadata(&helper.bios_);
  const string text = "### Testing various character attributes!! ###";
  for (size_t i = 0, address = metadata->vram_address; i < text.size();
       ++i, address += 2) {
    WriteMemoryByte(&helper.bios_, address, text[i]);
    WriteMemoryByte(&helper.bios_, address + 1, 0x70);  // Reverse video
  }
  EXPECT_TRUE(helper.RenderToFileAndCheckGolden("mda_test_reverse"));

  // Now underline the text.
  for (size_t i = 0, address = metadata->vram_address; i < text.size();
       ++i, address += 2) {
    WriteMemoryByte(&helper.bios_, address + 1, 0x01);  // Underline
  }
  EXPECT_TRUE(helper.RenderToFileAndCheckGolden("mda_test_underline"));

  // Now set intense foreground color.
  for (size_t i = 0, address = metadata->vram_address; i < text.size();
       ++i, address += 2) {
    WriteMemoryByte(&helper.bios_, address + 1, 0x0F);  // Intense foreground
  }
  EXPECT_TRUE(helper.RenderToFileAndCheckGolden("mda_test_intense"));

  // Test intense + underline.
  for (size_t i = 0, address = metadata->vram_address; i < text.size();
       ++i, address += 2) {
    WriteMemoryByte(&helper.bios_, address + 1, 0x09);  // Intense + underline
  }
  EXPECT_TRUE(helper.RenderToFileAndCheckGolden("mda_test_intense_underline"));
}
