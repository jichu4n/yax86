#include "dma_test_helper.h"

namespace {

TEST_F(DMATest, InitialState) {
  // All channels should be masked on power-on.
  EXPECT_EQ(dma_.mask_register, 0x0F);
  // Status register should be clear.
  EXPECT_EQ(dma_.status_register, 0x00);
  // Flip-flop should be cleared.
  EXPECT_EQ(dma_.rw_byte, kDMARegisterLSB);
}

TEST_F(DMATest, WriteAndReadAddress) {
  // Write 0x1234 to channel 2 address register.
  DMAWritePort(&dma_, kDMAPortChannel2Address, 0x34);
  DMAWritePort(&dma_, kDMAPortChannel2Address, 0x12);

  // Reset flip-flop to read from LSB first.
  DMAWritePort(&dma_, kDMAPortFlipFlopReset, 0);

  // Read back the bytes.
  EXPECT_EQ(DMAReadPort(&dma_, kDMAPortChannel2Address), 0x34);
  EXPECT_EQ(DMAReadPort(&dma_, kDMAPortChannel2Address), 0x12);
}

TEST_F(DMATest, WriteAndReadCount) {
  // Write 0x5678 to channel 3 count register.
  DMAWritePort(&dma_, kDMAPortChannel3Count, 0x78);
  DMAWritePort(&dma_, kDMAPortChannel3Count, 0x56);

  // Reset flip-flop to read from LSB first.
  DMAWritePort(&dma_, kDMAPortFlipFlopReset, 0);

  // Read back the bytes.
  EXPECT_EQ(DMAReadPort(&dma_, kDMAPortChannel3Count), 0x78);
  EXPECT_EQ(DMAReadPort(&dma_, kDMAPortChannel3Count), 0x56);
}

TEST_F(DMATest, MasterReset) {
  // Change some state.
  dma_.mask_register = 0x05;
  dma_.command_register = 0xFF;

  // Issue master reset.
  DMAWritePort(&dma_, kDMAPortMasterReset, 0);

  // Verify state is reset.
  EXPECT_EQ(dma_.mask_register, 0x0F);
  EXPECT_EQ(dma_.command_register, 0x00);
}

TEST_F(DMATest, WriteModeRegister) {
  const uint8_t mode = kDMAModeSelectChannel1 | kDMAModeTransferTypeRead |
                       kDMAModeSingle | kDMAModeAutoInitialize;
  DMAWritePort(&dma_, kDMAPortMode, mode);

  EXPECT_EQ(dma_.channels[1].mode, mode);
}

TEST_F(DMATest, WriteMaskRegisters) {
  // Initial state is all masked (0x0F).
  EXPECT_EQ(dma_.mask_register, 0x0F);

  // Clear the mask for channel 2.
  DMAWritePort(&dma_, kDMAPortSingleMask, kDMAModeSelectChannel2);
  EXPECT_EQ(dma_.mask_register, 0b1011);

  // Set the mask for channel 2 again.
  DMAWritePort(&dma_, kDMAPortSingleMask, kDMAModeSelectChannel2 | (1 << 2));
  EXPECT_EQ(dma_.mask_register, 0b1111);

  // Use the all-mask register to set a new pattern.
  DMAWritePort(&dma_, kDMAPortAllMask, 0b0101);
  EXPECT_EQ(dma_.mask_register, 0b0101);
}

TEST_F(DMATest, ReadStatusRegisterClearsTC) {
  // Manually set a TC bit.
  dma_.status_register = (1 << 2);

  // Read the status register.
  EXPECT_EQ(DMAReadPort(&dma_, kDMAPortCommandStatus), (1 << 2));

  // Verify the status register is now clear.
  EXPECT_EQ(dma_.status_register, 0x00);
}

}  // namespace
