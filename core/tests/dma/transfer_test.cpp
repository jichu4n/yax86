#include "dma_test_helper.h"

namespace {

class DMATransferTest : public DMATest {
 protected:
  void SetUpChannel2ForTransfer(uint8_t mode, uint16_t count) {
    // Set mode for channel 2.
    DMAWritePort(
        &dma_, kDMAPortMode, kDMAModeSelectChannel2 | mode);

    // Set page, address, and count.
    DMAWritePort(&dma_, kDMAPortPageChannel2, 0x01);
    DMAWritePort(&dma_, kDMAPortChannel2Address, 0x34);
    DMAWritePort(&dma_, kDMAPortChannel2Address, 0x12);
    DMAWritePort(&dma_, kDMAPortChannel2Count, (count - 1) & 0xFF);
    DMAWritePort(&dma_, kDMAPortChannel2Count, ((count - 1) >> 8) & 0xFF);

    // Unmask channel 2.
    DMAWritePort(&dma_, kDMAPortSingleMask, kDMAModeSelectChannel2);
  }
};

TEST_F(DMATransferTest, MemoryWriteTransfer) {
  // Arrange: Configure Ch 2 for a memory write of 1 byte.
  SetUpChannel2ForTransfer(kDMAModeTransferTypeWrite, 1);
  g_data_from_device = 0xAB;

  // Act: Perform the transfer.
  DMATransferByte(&dma_, 2);

  // Assert: Check that the data was written to the correct memory location.
  const uint32_t expected_address = 0x011234;
  EXPECT_EQ(g_mock_memory[expected_address], 0xAB);
}

TEST_F(DMATransferTest, MemoryReadTransfer) {
  // Arrange: Configure Ch 2 for a memory read of 1 byte.
  SetUpChannel2ForTransfer(kDMAModeTransferTypeRead, 1);
  const uint32_t expected_address = 0x011234;
  g_mock_memory[expected_address] = 0xCD;

  // Act: Perform the transfer.
  DMATransferByte(&dma_, 2);

  // Assert: Check that the data was "sent" to the device.
  EXPECT_EQ(g_data_to_device, 0xCD);
}

TEST_F(DMATransferTest, AddressDecrement) {
  // Arrange: Configure Ch 2 for a memory write with address decrement.
  SetUpChannel2ForTransfer(kDMAModeTransferTypeWrite | kDMAModeAddressDecrement, 1);

  // Act: Perform the transfer.
  DMATransferByte(&dma_, 2);

  // Assert: The address should have been decremented.
  EXPECT_EQ(dma_.channels[2].current_address, 0x1233);
}

TEST_F(DMATransferTest, TerminalCount) {
  // Arrange: Configure for a 1-byte transfer.
  SetUpChannel2ForTransfer(kDMAModeTransferTypeWrite, 1);

  // Act: Perform the transfer. Count is now 0xFFFF.
  DMATransferByte(&dma_, 2);

  // Assert: TC bit for channel 2 should be set in status register.
  EXPECT_EQ(dma_.status_register, (1 << 2));
  // Assert: Channel 2 should now be masked.
  EXPECT_EQ(dma_.mask_register & (1 << 2), (1 << 2));
}

TEST_F(DMATransferTest, AutoInitialize) {
  // Arrange: Configure for a 1-byte transfer with auto-initialize.
  SetUpChannel2ForTransfer(kDMAModeTransferTypeWrite | kDMAModeAutoInitialize, 1);

  // Act: Perform the transfer.
  DMATransferByte(&dma_, 2);

  // Assert: TC bit should be set.
  EXPECT_EQ(dma_.status_register, (1 << 2));
  // Assert: Channel should NOT be masked.
  EXPECT_EQ(dma_.mask_register & (1 << 2), 0);
  // Assert: Current address and count should be reset to their base values.
  EXPECT_EQ(dma_.channels[2].current_address, dma_.channels[2].base_address);
  EXPECT_EQ(dma_.channels[2].current_count, dma_.channels[2].base_count);
}

TEST_F(DMATransferTest, MaskedChannelBlocksTransfer) {
  // Arrange: Configure for a transfer but keep the channel masked.
  SetUpChannel2ForTransfer(kDMAModeTransferTypeWrite, 1);
  DMAWritePort(&dma_, kDMAPortSingleMask, kDMAModeSelectChannel2 | (1 << 2));
  g_data_from_device = 0xAB;

  // Act: Attempt the transfer.
  DMATransferByte(&dma_, 2);

  // Assert: No data should have been written to memory.
  const uint32_t expected_address = 0x011234;
  EXPECT_EQ(g_mock_memory[expected_address], 0x00);
}

}  // namespace
