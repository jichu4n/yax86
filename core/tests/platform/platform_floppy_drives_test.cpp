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

}  // namespace
