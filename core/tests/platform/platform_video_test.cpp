// Tests that the platform wires up the video module correctly.
#include "gtest/gtest.h"
#include "platform.h"

namespace {

class PlatformVideoTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.physical_memory_size = 64 * 1024;
    config_.read_physical_memory_byte =
        [](PlatformState*, uint32_t) -> uint8_t { return 0xFF; };
    config_.write_physical_memory_byte = [](PlatformState*, uint32_t, uint8_t) {
    };
    ASSERT_TRUE(PlatformInit(&platform_, &config_));
  }

  PlatformConfig config_ = {0};
  PlatformState platform_ = {0};
};

TEST_F(PlatformVideoTest, RegistersVideo) {
  EXPECT_EQ(platform_.ppi_config.display_mode, kPPIDisplayMDA);

  MemoryMapEntry* vram =
      GetMemoryMapEntryByType(&platform_, kMemoryMapEntryVRAM);
  ASSERT_NE(vram, nullptr);
  EXPECT_EQ(vram->start, kMDAModeMetadata.vram_address);
  EXPECT_EQ(
      vram->end,
      kMDAModeMetadata.vram_address + kMDAModeMetadata.vram_size - 1);

  PortMapEntry* ports = GetPortMapEntryByType(&platform_, kPortMapEntryVideo);
  ASSERT_NE(ports, nullptr);
  EXPECT_EQ(ports->start, kMDAPortStart);
  EXPECT_EQ(ports->end, kMDAPortEnd);
}

TEST_F(PlatformVideoTest, VideoPortsAreRoutedThroughTheMap) {
  WritePortByte(&platform_, kMDAPortRegisterIndex, kCRTCRegisterCursorL);
  EXPECT_EQ(platform_.video.selected_register, kCRTCRegisterCursorL);
  WritePortByte(&platform_, kMDAPortRegisterData, 0x23);
  EXPECT_EQ(ReadPortByte(&platform_, kMDAPortRegisterData), 0x23);
}

TEST_F(PlatformVideoTest, TicksAdvanceTheBeam) {
  uint32_t start_ticks = platform_.ticks;
  while (platform_.ticks - start_ticks < kMDACyclesPerScanLine * 4) {
    PlatformTick(&platform_);
  }
  EXPECT_GT(platform_.video.scan_line, 0);
}

}  // namespace
