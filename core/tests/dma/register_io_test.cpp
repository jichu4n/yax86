#include "dma_test_helper.h"

namespace {

TEST_F(DMATest, InitialState) {
  // All channels should be masked on power-on.
  EXPECT_EQ(dma_.mask_register, 0x0F);
  // Status register should be clear.
  EXPECT_EQ(dma_.status_register, 0x00);
  // Flip-flop should be cleared.
  EXPECT_EQ(dma_.rw_byte, Y_DMA_REGISTER_LSB);
}

TEST_F(DMATest, WriteAndReadAddress) {
  // Write 0x1234 to channel 2 address register.
  YDMAWritePort(&dma_, Y_DMA_PORT_CHANNEL2_ADDRESS, 0x34);
  YDMAWritePort(&dma_, Y_DMA_PORT_CHANNEL2_ADDRESS, 0x12);

  // Reset flip-flop to read from LSB first.
  YDMAWritePort(&dma_, Y_DMA_PORT_FLIP_FLOP_RESET, 0);

  // Read back the bytes.
  EXPECT_EQ(YDMAReadPort(&dma_, Y_DMA_PORT_CHANNEL2_ADDRESS), 0x34);
  EXPECT_EQ(YDMAReadPort(&dma_, Y_DMA_PORT_CHANNEL2_ADDRESS), 0x12);
}

TEST_F(DMATest, WriteAndReadCount) {
  // Write 0x5678 to channel 3 count register.
  YDMAWritePort(&dma_, Y_DMA_PORT_CHANNEL3_COUNT, 0x78);
  YDMAWritePort(&dma_, Y_DMA_PORT_CHANNEL3_COUNT, 0x56);

  // Reset flip-flop to read from LSB first.
  YDMAWritePort(&dma_, Y_DMA_PORT_FLIP_FLOP_RESET, 0);

  // Read back the bytes.
  EXPECT_EQ(YDMAReadPort(&dma_, Y_DMA_PORT_CHANNEL3_COUNT), 0x78);
  EXPECT_EQ(YDMAReadPort(&dma_, Y_DMA_PORT_CHANNEL3_COUNT), 0x56);
}

TEST_F(DMATest, MasterReset) {
  // Change some state.
  dma_.mask_register = 0x05;
  dma_.command_register = 0xFF;

  // Issue master reset.
  YDMAWritePort(&dma_, Y_DMA_PORT_MASTER_RESET, 0);

  // Verify state is reset.
  EXPECT_EQ(dma_.mask_register, 0x0F);
  EXPECT_EQ(dma_.command_register, 0x00);
}

TEST_F(DMATest, WriteModeRegister) {
  const uint8_t mode = Y_DMA_MODE_SELECT_CHANNEL1 |
                       Y_DMA_MODE_TRANSFER_TYPE_READ | Y_DMA_MODE_SINGLE |
                       Y_DMA_MODE_AUTO_INITIALIZE;
  YDMAWritePort(&dma_, Y_DMA_PORT_MODE, mode);

  EXPECT_EQ(dma_.channels[1].mode, mode);
}

TEST_F(DMATest, WriteMaskRegisters) {
  // Initial state is all masked (0x0F).
  EXPECT_EQ(dma_.mask_register, 0x0F);

  // Clear the mask for channel 2.
  YDMAWritePort(&dma_, Y_DMA_PORT_SINGLE_MASK, Y_DMA_MODE_SELECT_CHANNEL2);
  EXPECT_EQ(dma_.mask_register, 0b1011);

  // Set the mask for channel 2 again.
  YDMAWritePort(
      &dma_, Y_DMA_PORT_SINGLE_MASK, Y_DMA_MODE_SELECT_CHANNEL2 | (1 << 2));
  EXPECT_EQ(dma_.mask_register, 0b1111);

  // Use the all-mask register to set a new pattern.
  YDMAWritePort(&dma_, Y_DMA_PORT_ALL_MASK, 0b0101);
  EXPECT_EQ(dma_.mask_register, 0b0101);
}

TEST_F(DMATest, ReadStatusRegisterClearsTC) {
  // Manually set a TC bit.
  dma_.status_register = (1 << 2);

  // Read the status register.
  EXPECT_EQ(YDMAReadPort(&dma_, Y_DMA_PORT_COMMAND_STATUS), (1 << 2));

  // Verify the status register is now clear.
  EXPECT_EQ(dma_.status_register, 0x00);
}

}  // namespace
