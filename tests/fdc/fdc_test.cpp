#include "gtest/gtest.h"
#include "fdc.h"

namespace {

class FDCTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.context = this;
    config_.raise_irq6 = [](void* context) {
      static_cast<FDCTest*>(context)->irq6_raised_ = true;
    };
    config_.read_image_byte = NULL;
    config_.write_image_byte = NULL;

    FDCInit(&fdc_, &config_);
  }

  void SendCommand(uint8_t cmd) {
    FDCWritePort(&fdc_, kFDCPortData, cmd);
  }

  void SendParameter(uint8_t param) {
    FDCWritePort(&fdc_, kFDCPortData, param);
  }

  uint8_t ReadResult() {
    return FDCReadPort(&fdc_, kFDCPortData);
  }

  FDCConfig config_;
  FDCState fdc_;
  bool irq6_raised_ = false;
};

TEST_F(FDCTest, RecalibrateAndSenseInterruptStatus) {
  // 1. Issue Recalibrate command for Drive 0.
  irq6_raised_ = false;
  SendCommand(0x07); // Recalibrate
  SendParameter(0x00); // Drive 0

  // Tick the FDC to process the command. Recalibrate needs at least 2 ticks
  // (start seek, finish seek).
  FDCTick(&fdc_);
  FDCTick(&fdc_);

  // Verify IRQ6 was raised.
  EXPECT_TRUE(irq6_raised_);
  EXPECT_EQ(fdc_.phase, kFDCPhaseIdle);

  // 2. Issue Sense Interrupt Status command.
  SendCommand(0x08); // Sense Interrupt Status

  // Tick to execute Sense Interrupt Status.
  FDCTick(&fdc_);

  // No execution phase for Sense Interrupt Status, goes straight to Result.
  // Actually, wait, Sense Interrupt Status handler finishes execution immediately.
  // But FDCFinishCommandExecution transitions to kFDCPhaseResult if there are bytes.
  
  // Actually, Sense Interrupt Status result bytes are available immediately.
  // Read ST0.
  uint8_t st0 = ReadResult();
  // Bits 7-6: 00 (Normal Termination)
  // Bit 5: 1 (Seek End)
  // Bits 1-0: 00 (Drive 0)
  // Expected: 00100000 = 0x20
  EXPECT_EQ(st0, 0x20);

  // Read PCN (Present Cylinder Number).
  uint8_t pcn = ReadResult();
  EXPECT_EQ(pcn, 0x00); // Should be 0 after recalibrate.

  // Verify we are back to Idle.
  EXPECT_EQ(fdc_.phase, kFDCPhaseIdle);
}

TEST_F(FDCTest, SenseInterruptStatusNoPending) {
  // Issue Sense Interrupt Status command without any prior Seek/Recalibrate.
  SendCommand(0x08); // Sense Interrupt Status

  // Tick to execute.
  FDCTick(&fdc_);

  // Read ST0.
  uint8_t st0 = ReadResult();
  // Should be Invalid Command (0x80).
  EXPECT_EQ(st0, 0x80);

  // Verify we are back to Idle.
  EXPECT_EQ(fdc_.phase, kFDCPhaseIdle);
}

} // namespace
