#include <gtest/gtest.h>

#include "ppi.h"

namespace {

class PortCTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Initialize config to zero and then init the PPI state.
    config_ = {0};
    PPIInit(&ppi_, &config_);
  }

  PPIState ppi_ = {0};
  PPIConfig config_ = {0};
};

TEST_F(PortCTest, ReadSwitches1to4) {
  // Ensure the DIP switch select bit is 0.
  ppi_.port_b = 0;

  // Test Case 1: 1 FDD, no FPU, 256KB RAM
  config_.num_floppy_drives = 1;
  config_.fpu_installed = false;
  config_.memory_size = kPPIMemorySize256KB;
  // Expected: 0b00001101
  // Bit 0 (FDD): 1
  // Bit 1 (FPU): 0
  // Bits 2-3 (RAM): 11 (kPPIMemorySize256KB = 3)
  EXPECT_EQ(PPIReadPort(&ppi_, kPPIPortC), 0b00001101);

  // Test Case 2: 0 FDD, FPU, 64KB RAM
  config_.num_floppy_drives = 0;
  config_.fpu_installed = true;
  config_.memory_size = kPPIMemorySize64KB;
  // Expected: 0b00000010
  // Bit 0 (FDD): 0 (num_floppy_drives is not > 0)
  // Bit 1 (FPU): 1
  // Bits 2-3 (RAM): 00 (kPPIMemorySize64KB = 0)
  EXPECT_EQ(PPIReadPort(&ppi_, kPPIPortC), 0b00000010);

  // Test Case 3: 4 FDDs, FPU, 192KB RAM
  config_.num_floppy_drives = 4;
  config_.fpu_installed = true;
  config_.memory_size = kPPIMemorySize192KB;
  // Expected: 0b00001011
  // Bit 0 (FDD): 1
  // Bit 1 (FPU): 1
  // Bits 2-3 (RAM): 10 (kPPIMemorySize192KB = 2)
  EXPECT_EQ(PPIReadPort(&ppi_, kPPIPortC), 0b00001011);
}

TEST_F(PortCTest, ReadSwitches5to8) {
  // Set the DIP switch select bit to 1.
  PPIWritePort(&ppi_, kPPIPortB, kPPIPortBDipSwitchSelect);
  EXPECT_EQ(ppi_.port_b, kPPIPortBDipSwitchSelect);

  // Test Case 1: 2 FDDs, CGA 80x25
  config_.num_floppy_drives = 2;
  config_.display_mode = kPPIDisplayCGA80x25;
  // Expected: 0b00000110
  // Bits 0-1 (Video): 10 (kPPIDisplayCGA80x25 = 2)
  // Bits 2-3 (FDDs): 01 (2 drives - 1 = 1)
  EXPECT_EQ(PPIReadPort(&ppi_, kPPIPortC), 0b00000110);

  // Test Case 2: 4 FDDs, MDA
  config_.num_floppy_drives = 4;
  config_.display_mode = kPPIDisplayMDA;
  // Expected: 0b00001111
  // Bits 0-1 (Video): 11 (kPPIDisplayMDA = 3)
  // Bits 2-3 (FDDs): 11 (4 drives - 1 = 3)
  EXPECT_EQ(PPIReadPort(&ppi_, kPPIPortC), 0b00001111);

  // Test Case 3 (Clamping): 0 FDDs (clamps to 1), EGA
  config_.num_floppy_drives = 0;
  config_.display_mode = kPPIDisplayEGA;
  // Expected: 0b00000000
  // Bits 0-1 (Video): 00 (kPPIDisplayEGA = 0)
  // Bits 2-3 (FDDs): 00 (clamped 1 drive - 1 = 0)
  EXPECT_EQ(PPIReadPort(&ppi_, kPPIPortC), 0b00000000);

  // Test Case 4 (Clamping): 5 FDDs (clamps to 4), CGA 40x25
  config_.num_floppy_drives = 5;
  config_.display_mode = kPPIDisplayCGA40x25;
  // Expected: 0b00001101
  // Bits 0-1 (Video): 01 (kPPIDisplayCGA40x25 = 1)
  // Bits 2-3 (FDDs): 11 (clamped 4 drives - 1 = 3)
  EXPECT_EQ(PPIReadPort(&ppi_, kPPIPortC), 0b00001101);
}

}  // namespace
