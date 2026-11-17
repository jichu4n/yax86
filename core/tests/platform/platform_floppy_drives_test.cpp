#include "gtest/gtest.h"
#include "platform.h"

namespace {

// The DIP switch bits that give the number of floppy drives, read from PPI
// port C with the high switches selected: one less than the count.
enum : uint8_t {
  kDriveCountShift = 2,
  kDriveCountMask = 0x03 << kDriveCountShift,
};

// Checks that YPlatformConfig.num_floppy_drives reaches the BIOS through the
// PPI's DIP switches, as the guest reads them.
class PlatformFloppyDrivesTest : public ::testing::Test {
 protected:
  void Init(uint8_t num_floppy_drives) {
    platform_.config.direct_memory_size = sizeof(ram_);
    platform_.config.direct_memory = ram_;
    platform_.config.vram = vram_;
    platform_.config.num_floppy_drives = num_floppy_drives;
    ASSERT_TRUE(YPlatformInit(&platform_));
  }

  // The number of drives the switches report.
  uint8_t ReportedDrives() {
    YPlatformWritePortByte(
        &platform_, Y_PPI_PORT_B, Y_PPI_PORT_B_DIP_SWITCH_SELECT);
    const uint8_t switches = YPlatformReadPortByte(&platform_, Y_PPI_PORT_C);
    return ((switches & kDriveCountMask) >> kDriveCountShift) + 1;
  }

  YPlatformState platform_ = {};
  uint8_t ram_[64 * 1024] = {0};
  uint8_t vram_[Y_VIDEO_CGA_VRAM_SIZE] = {0};
};

TEST_F(PlatformFloppyDrivesTest, UnsetIsOneDrive) {
  Init(0);
  EXPECT_EQ(ReportedDrives(), 1);
}

TEST_F(PlatformFloppyDrivesTest, TwoDrives) {
  Init(2);
  EXPECT_EQ(ReportedDrives(), 2);
}

TEST_F(PlatformFloppyDrivesTest, UnsetDriveTypesDoesNotMapFloppyOptionROM) {
  Init(2);
  EXPECT_EQ(
      YPlatformGetMemoryMapEntryByType(
          &platform_, Y_PLATFORM_MEMORY_MAP_ENTRY_FDC_OPTION_ROM),
      nullptr);
}

TEST_F(PlatformFloppyDrivesTest, ConfiguredDriveTypesMapsValidFloppyOptionROM) {
  platform_.config.floppy_drive_types[0] = Y_FDC_DRIVE_TYPE_1440KB;
  platform_.config.floppy_drive_types[1] = Y_FDC_DRIVE_TYPE_1200KB;
  Init(0);

  EXPECT_EQ(ReportedDrives(), 2);
  const YPlatformMemoryMapEntry* entry = YPlatformGetMemoryMapEntryByType(
      &platform_, Y_PLATFORM_MEMORY_MAP_ENTRY_FDC_OPTION_ROM);
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->start, (uint32_t)Y_FDC_OPTION_ROM_START_ADDRESS);
  EXPECT_EQ(
      entry->end,
      (uint32_t)Y_FDC_OPTION_ROM_START_ADDRESS + YFDCGetOptionROMSize() - 1);

  EXPECT_EQ(
      YPlatformReadMemoryWord(&platform_, Y_FDC_OPTION_ROM_START_ADDRESS),
      0xAA55);
  // Drive 0 and 1 CMOS types in the configuration block at 0x1FC0 + 1 and + 5.
  EXPECT_EQ(
      YPlatformReadMemoryByte(
          &platform_,
          Y_FDC_OPTION_ROM_START_ADDRESS + Y_FDC_OPTION_ROM_CONFIG_OFFSET + 1),
      Y_FDC_DRIVE_TYPE_1440KB);
  EXPECT_EQ(
      YPlatformReadMemoryByte(
          &platform_,
          Y_FDC_OPTION_ROM_START_ADDRESS + Y_FDC_OPTION_ROM_CONFIG_OFFSET + 5),
      Y_FDC_DRIVE_TYPE_1200KB);
  // config_on_boot (0x04) in config_flags (0x1FEB) must be cleared.
  EXPECT_EQ(
      YPlatformReadMemoryByte(
          &platform_, Y_FDC_OPTION_ROM_START_ADDRESS + 0x1FEB) &
          0x04,
      0);

  // Both the 64-byte config block and the whole 8KB option ROM must sum to 0.
  uint8_t config_sum = 0;
  for (uint32_t i = 0; i < Y_FDC_OPTION_ROM_CONFIG_SIZE; ++i) {
    config_sum =
        (uint8_t)(config_sum +
                  YPlatformReadMemoryByte(
                      &platform_, Y_FDC_OPTION_ROM_START_ADDRESS +
                                      Y_FDC_OPTION_ROM_CONFIG_OFFSET + i));
  }
  EXPECT_EQ(config_sum, 0);

  uint8_t rom_sum = 0;
  for (uint32_t i = 0; i < YFDCGetOptionROMSize(); ++i) {
    rom_sum = (uint8_t)(rom_sum +
                        YPlatformReadMemoryByte(
                            &platform_, Y_FDC_OPTION_ROM_START_ADDRESS + i));
  }
  EXPECT_EQ(rom_sum, 0);
}

}  // namespace
