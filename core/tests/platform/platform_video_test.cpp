// Tests that the platform wires up whichever video adapter it is configured
// with.
#include "gtest/gtest.h"
#include "platform.h"

namespace {

class PlatformVideoTest : public ::testing::Test {
 protected:
  void Init(YVideoAdapter adapter) {
    platform_.config.physical_memory_size = sizeof(ram_);
    platform_.config.physical_memory = ram_;
    platform_.config.vram = vram_;
    platform_.config.video_adapter = adapter;
    ASSERT_TRUE(YPlatformInit(&platform_));
  }

  YPlatformState platform_ = {0};
  uint8_t ram_[64 * 1024] = {0};
  uint8_t vram_[Y_VIDEO_CGA_VRAM_SIZE] = {0};
};

TEST_F(PlatformVideoTest, DefaultsToMDA) {
  Init(static_cast<YVideoAdapter>(0));
  EXPECT_EQ(platform_.video.adapter, Y_VIDEO_ADAPTER_MDA);
  EXPECT_EQ(platform_.ppi.config.display_mode, Y_PPI_DISPLAY_MDA);

  YPlatformMemoryMapEntry* vram = YPlatformGetMemoryMapEntryByType(
      &platform_, Y_PLATFORM_MEMORY_MAP_ENTRY_VRAM);
  ASSERT_NE(vram, nullptr);
  EXPECT_EQ(vram->start, static_cast<uint32_t>(Y_VIDEO_MDA_VRAM_ADDRESS));
  EXPECT_EQ(
      vram->end, static_cast<uint32_t>(
                     Y_VIDEO_MDA_VRAM_ADDRESS + Y_VIDEO_MDA_VRAM_SIZE - 1));

  YPlatformPortMapEntry* ports = YPlatformGetPortMapEntryByType(
      &platform_, Y_PLATFORM_PORT_MAP_ENTRY_VIDEO);
  ASSERT_NE(ports, nullptr);
  EXPECT_EQ(ports->start, Y_VIDEO_MDA_PORT_START);
  EXPECT_EQ(ports->end, Y_VIDEO_MDA_PORT_END);
}

TEST_F(PlatformVideoTest, RegistersCGA) {
  Init(Y_VIDEO_ADAPTER_CGA);
  EXPECT_EQ(platform_.video.adapter, Y_VIDEO_ADAPTER_CGA);
  // The BIOS branches on the DIP switches to decide which adapter to program,
  // so they have to agree with what the platform registered.
  EXPECT_EQ(platform_.ppi.config.display_mode, Y_PPI_DISPLAY_CGA_80X25);

  YPlatformMemoryMapEntry* vram = YPlatformGetMemoryMapEntryByType(
      &platform_, Y_PLATFORM_MEMORY_MAP_ENTRY_VRAM);
  ASSERT_NE(vram, nullptr);
  EXPECT_EQ(vram->start, static_cast<uint32_t>(Y_VIDEO_CGA_VRAM_ADDRESS));
  EXPECT_EQ(
      vram->end, static_cast<uint32_t>(
                     Y_VIDEO_CGA_VRAM_ADDRESS + Y_VIDEO_CGA_VRAM_SIZE - 1));

  YPlatformPortMapEntry* ports = YPlatformGetPortMapEntryByType(
      &platform_, Y_PLATFORM_PORT_MAP_ENTRY_VIDEO);
  ASSERT_NE(ports, nullptr);
  EXPECT_EQ(ports->start, Y_VIDEO_CGA_PORT_START);
  EXPECT_EQ(ports->end, Y_VIDEO_CGA_PORT_END);
}

// YPlatformInit() replaces the video config wholesale, so a font only reaches
// the adapter if the platform passes it on.
TEST_F(PlatformVideoTest, PassesTheTextFontToTheAdapter) {
  static const uint8_t kGlyphs[Y_VIDEO_TEXT_FONT_GLYPHS * 10] = {0};
  platform_.config.video_text_font.glyphs = kGlyphs;
  platform_.config.video_text_font.width = 4;
  platform_.config.video_text_font.height = 10;
  Init(Y_VIDEO_ADAPTER_CGA);

  EXPECT_TRUE(platform_.video.has_text_font);
  const YVideoSize frame = YVideoGetFrameSize(&platform_.video);
  EXPECT_EQ(frame.width, 320);
  EXPECT_EQ(frame.height, 250);
}

TEST_F(PlatformVideoTest, VideoPortsAreRoutedThroughTheMap) {
  Init(Y_VIDEO_ADAPTER_CGA);
  // The 6845 index register is reachable at the CGA's port addresses, and the
  // MDA's are not mapped at all.
  YPlatformWritePortByte(
      &platform_, Y_VIDEO_CGA_PORT_REGISTER_INDEX,
      Y_VIDEO_CRTC_REGISTER_CURSOR_L);
  EXPECT_EQ(platform_.video.selected_register, Y_VIDEO_CRTC_REGISTER_CURSOR_L);
  YPlatformWritePortByte(&platform_, Y_VIDEO_CGA_PORT_REGISTER_DATA, 0x23);
  EXPECT_EQ(
      YPlatformReadPortByte(&platform_, Y_VIDEO_CGA_PORT_REGISTER_DATA), 0x23);
  EXPECT_EQ(
      YPlatformGetPortMapEntryForPort(
          &platform_, Y_VIDEO_MDA_PORT_REGISTER_INDEX),
      nullptr);
}

TEST_F(PlatformVideoTest, TicksAdvanceTheBeam) {
  Init(Y_VIDEO_ADAPTER_CGA);
  const YVideoAdapterMetadata* adapter =
      YVideoGetAdapterMetadata(&platform_.video);
  uint32_t start_ticks = platform_.ticks;
  while (platform_.ticks - start_ticks < adapter->cycles_per_scan_line * 4) {
    YPlatformTick(&platform_);
  }
  EXPECT_GT(platform_.video.scan_line, 0);
}

}  // namespace
