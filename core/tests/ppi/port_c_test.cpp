#include <gtest/gtest.h>

#include "ppi.h"

namespace {

class PortCTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Initialize config to zero and then init the PPI state.
    ppi_ = {};
    YPPIInit(&ppi_);
  }

  YPPIState ppi_ = {0};
};

TEST_F(PortCTest, ReadSwitches1to4) {
  // Ensure the DIP switch select bit is 0.
  ppi_.port_b = 0;

  // Test Case 1: 1 FDD, no FPU, 256KB RAM
  ppi_.config.num_floppy_drives = 1;
  ppi_.config.is_fpu_installed = false;
  ppi_.config.memory_size = Y_PPI_MEMORY_SIZE_256KB;
  // Expected: 0b00001101
  // Bit 0 (FDD): 1
  // Bit 1 (FPU): 0
  // Bits 2-3 (RAM): 11 (Y_PPI_MEMORY_SIZE_256KB = 3)
  EXPECT_EQ(YPPIReadPort(&ppi_, Y_PPI_PORT_C), 0b00001101);

  // Test Case 2: 0 FDD, FPU, 64KB RAM
  ppi_.config.num_floppy_drives = 0;
  ppi_.config.is_fpu_installed = true;
  ppi_.config.memory_size = Y_PPI_MEMORY_SIZE_64KB;
  // Expected: 0b00000010
  // Bit 0 (FDD): 0 (num_floppy_drives is not > 0)
  // Bit 1 (FPU): 1
  // Bits 2-3 (RAM): 00 (Y_PPI_MEMORY_SIZE_64KB = 0)
  EXPECT_EQ(YPPIReadPort(&ppi_, Y_PPI_PORT_C), 0b00000010);

  // Test Case 3: 4 FDDs, FPU, 192KB RAM
  ppi_.config.num_floppy_drives = 4;
  ppi_.config.is_fpu_installed = true;
  ppi_.config.memory_size = Y_PPI_MEMORY_SIZE_192KB;
  // Expected: 0b00001011
  // Bit 0 (FDD): 1
  // Bit 1 (FPU): 1
  // Bits 2-3 (RAM): 10 (Y_PPI_MEMORY_SIZE_192KB = 2)
  EXPECT_EQ(YPPIReadPort(&ppi_, Y_PPI_PORT_C), 0b00001011);
}

TEST_F(PortCTest, ReadSwitches5to8) {
  // Set the DIP switch select bit to 1.
  YPPIWritePort(&ppi_, Y_PPI_PORT_B, Y_PPI_PORT_B_DIP_SWITCH_SELECT);
  EXPECT_EQ(ppi_.port_b, Y_PPI_PORT_B_DIP_SWITCH_SELECT);

  // Test Case 1: 2 FDDs, CGA 80x25
  ppi_.config.num_floppy_drives = 2;
  ppi_.config.display_mode = Y_PPI_DISPLAY_CGA_80X25;
  // Expected: 0b00000110
  // Bits 0-1 (Video): 10 (Y_PPI_DISPLAY_CGA_80X25 = 2)
  // Bits 2-3 (FDDs): 01 (2 drives - 1 = 1)
  EXPECT_EQ(YPPIReadPort(&ppi_, Y_PPI_PORT_C), 0b00000110);

  // Test Case 2: 4 FDDs, MDA
  ppi_.config.num_floppy_drives = 4;
  ppi_.config.display_mode = Y_PPI_DISPLAY_MDA;
  // Expected: 0b00001111
  // Bits 0-1 (Video): 11 (Y_PPI_DISPLAY_MDA = 3)
  // Bits 2-3 (FDDs): 11 (4 drives - 1 = 3)
  EXPECT_EQ(YPPIReadPort(&ppi_, Y_PPI_PORT_C), 0b00001111);

  // Test Case 3 (Clamping): 0 FDDs (clamps to 1), EGA
  ppi_.config.num_floppy_drives = 0;
  ppi_.config.display_mode = Y_PPI_DISPLAY_EGA;
  // Expected: 0b00000000
  // Bits 0-1 (Video): 00 (Y_PPI_DISPLAY_EGA = 0)
  // Bits 2-3 (FDDs): 00 (clamped 1 drive - 1 = 0)
  EXPECT_EQ(YPPIReadPort(&ppi_, Y_PPI_PORT_C), 0b00000000);

  // Test Case 4 (Clamping): 5 FDDs (clamps to 4), CGA 40x25
  ppi_.config.num_floppy_drives = 5;
  ppi_.config.display_mode = Y_PPI_DISPLAY_CGA_40X25;
  // Expected: 0b00001101
  // Bits 0-1 (Video): 01 (Y_PPI_DISPLAY_CGA_40X25 = 1)
  // Bits 2-3 (FDDs): 11 (clamped 4 drives - 1 = 3)
  EXPECT_EQ(YPPIReadPort(&ppi_, Y_PPI_PORT_C), 0b00001101);
}

}  // namespace
