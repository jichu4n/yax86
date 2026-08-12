// Tests that the platform wires up whichever video adapter it is configured
// with.
#include "gtest/gtest.h"
#include "platform.h"

namespace {

class PlatformVideoTest : public ::testing::Test {
 protected:
  void Init(VideoAdapter adapter) {
    config_.physical_memory_size = sizeof(ram_);
    config_.physical_memory = ram_;
    config_.video_adapter = adapter;
    ASSERT_TRUE(PlatformInit(&platform_, &config_));
  }

  PlatformConfig config_ = {0};
  PlatformState platform_ = {0};
  uint8_t ram_[64 * 1024] = {0};
};

TEST_F(PlatformVideoTest, DefaultsToMDA) {
  Init(static_cast<VideoAdapter>(0));
  EXPECT_EQ(platform_.video.adapter, kVideoAdapterMDA);
  EXPECT_EQ(platform_.ppi_config.display_mode, kPPIDisplayMDA);

  MemoryMapEntry* vram =
      GetMemoryMapEntryByType(&platform_, kMemoryMapEntryVRAM);
  ASSERT_NE(vram, nullptr);
  EXPECT_EQ(vram->start, static_cast<uint32_t>(kMDAVRAMAddress));
  EXPECT_EQ(
      vram->end, static_cast<uint32_t>(kMDAVRAMAddress + kMDAVRAMSize - 1));

  PortMapEntry* ports = GetPortMapEntryByType(&platform_, kPortMapEntryVideo);
  ASSERT_NE(ports, nullptr);
  EXPECT_EQ(ports->start, kMDAPortStart);
  EXPECT_EQ(ports->end, kMDAPortEnd);
}

TEST_F(PlatformVideoTest, RegistersCGA) {
  Init(kVideoAdapterCGA);
  EXPECT_EQ(platform_.video.adapter, kVideoAdapterCGA);
  // The BIOS branches on the DIP switches to decide which adapter to program,
  // so they have to agree with what the platform registered.
  EXPECT_EQ(platform_.ppi_config.display_mode, kPPIDisplayCGA80x25);

  MemoryMapEntry* vram =
      GetMemoryMapEntryByType(&platform_, kMemoryMapEntryVRAM);
  ASSERT_NE(vram, nullptr);
  EXPECT_EQ(vram->start, static_cast<uint32_t>(kCGAVRAMAddress));
  EXPECT_EQ(
      vram->end, static_cast<uint32_t>(kCGAVRAMAddress + kCGAVRAMSize - 1));

  PortMapEntry* ports = GetPortMapEntryByType(&platform_, kPortMapEntryVideo);
  ASSERT_NE(ports, nullptr);
  EXPECT_EQ(ports->start, kCGAPortStart);
  EXPECT_EQ(ports->end, kCGAPortEnd);
}

TEST_F(PlatformVideoTest, VideoPortsAreRoutedThroughTheMap) {
  Init(kVideoAdapterCGA);
  // The 6845 index register is reachable at the CGA's port addresses, and the
  // MDA's are not mapped at all.
  WritePortByte(&platform_, kCGAPortRegisterIndex, kCRTCRegisterCursorL);
  EXPECT_EQ(platform_.video.selected_register, kCRTCRegisterCursorL);
  WritePortByte(&platform_, kCGAPortRegisterData, 0x23);
  EXPECT_EQ(ReadPortByte(&platform_, kCGAPortRegisterData), 0x23);
  EXPECT_EQ(GetPortMapEntryForPort(&platform_, kMDAPortRegisterIndex), nullptr);
}

TEST_F(PlatformVideoTest, TicksAdvanceTheBeam) {
  Init(kVideoAdapterCGA);
  const VideoAdapterMetadata* adapter =
      VideoGetAdapterMetadata(&platform_.video);
  uint32_t start_ticks = platform_.ticks;
  while (platform_.ticks - start_ticks < adapter->cycles_per_scan_line * 4) {
    PlatformTick(&platform_);
  }
  EXPECT_GT(platform_.video.scan_line, 0);
}

}  // namespace
