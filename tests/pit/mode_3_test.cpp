#include <gtest/gtest.h>

#include "pit.h"

namespace {

// Mock callback functions and trackers
static int irq_0_call_count;
static void MockRaiseIRQ0(void* context) {
  irq_0_call_count++;
}

static uint32_t speaker_frequency_hz;
static void MockSetSpeakerFrequency(void* context, uint32_t frequency_hz) {
  speaker_frequency_hz = frequency_hz;
}

class Mode3Test : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.context = nullptr;
    config_.raise_irq_0 = MockRaiseIRQ0;
    config_.set_pc_speaker_frequency = MockSetSpeakerFrequency;
    PITInit(&pit_, &config_);

    // Reset mock trackers
    irq_0_call_count = 0;
    speaker_frequency_hz = 0;
  }

  PITConfig config_ = {0};
  PITState pit_ = {0};
};

TEST_F(Mode3Test, SystemTimerSquareWave) {
  // Configure Channel 0 for Mode 3, LSB/MSB access, with a reload value of 10000.
  // Control word: 0b00110110
  PITWritePort(&pit_, kPITPortControl, 0x36);

  // Write the 16-bit reload value.
  PITWritePort(&pit_, kPITPortChannel0, 0x10); // LSB of 10000 (0x2710)
  PITWritePort(&pit_, kPITPortChannel0, 0x27); // MSB

  // Initial state should be high output, no IRQ.
  EXPECT_TRUE(pit_.channels[0].output_state);
  EXPECT_EQ(irq_0_call_count, 0);

  // Tick for half the period (falling edge).
  for (int i = 0; i < 5000; ++i) {
    PITTick(&pit_);
  }
  EXPECT_FALSE(pit_.channels[0].output_state);
  EXPECT_EQ(irq_0_call_count, 0); // IRQ should NOT be raised on falling edge.

  // Tick for the second half of the period (rising edge).
  for (int i = 0; i < 5000; ++i) {
    PITTick(&pit_);
  }
  EXPECT_TRUE(pit_.channels[0].output_state);
  EXPECT_EQ(irq_0_call_count, 1); // IRQ SHOULD be raised on rising edge.

  // --- Second cycle to test periodicity ---
  irq_0_call_count = 0; // Reset for next cycle check.

  // Tick for the next falling edge.
  for (int i = 0; i < 5000; ++i) {
    PITTick(&pit_);
  }
  EXPECT_FALSE(pit_.channels[0].output_state);
  EXPECT_EQ(irq_0_call_count, 0);

  // Tick for the next rising edge.
  for (int i = 0; i < 5000; ++i) {
    PITTick(&pit_);
  }
  EXPECT_TRUE(pit_.channels[0].output_state);
  EXPECT_EQ(irq_0_call_count, 1);
}

TEST_F(Mode3Test, PCSpeakerFrequency) {
  // Configure Channel 2 for Mode 3, LSB/MSB access.
  // Control word: 0b10110110
  PITWritePort(&pit_, kPITPortControl, 0xB6);

  // Write a reload value of 1193, which should result in ~1000 Hz.
  PITWritePort(&pit_, kPITPortChannel2, 0xA9); // LSB of 1193 (0x04A9)
  PITWritePort(&pit_, kPITPortChannel2, 0x04); // MSB

  // The frequency callback should have been called with the correct frequency.
  // 1193182 / 1193 = 1000.15...
  EXPECT_EQ(speaker_frequency_hz, 1000);

  // Write a new reload value of 2386 (~500 Hz).
  PITWritePort(&pit_, kPITPortChannel2, 0x52); // LSB of 2386 (0x0952)
  PITWritePort(&pit_, kPITPortChannel2, 0x09); // MSB
  // 1193182 / 2386 = 500.07...
  EXPECT_EQ(speaker_frequency_hz, 500);
}

TEST_F(Mode3Test, LSBThenMSBReadWrite) {
  // Configure Channel 0 for Mode 3, LSB/MSB access.
  PITWritePort(&pit_, kPITPortControl, 0x36);

  // --- Test Write ---
  // Write LSB
  PITWritePort(&pit_, kPITPortChannel0, 0x12);
  EXPECT_EQ(pit_.channels[0].reload_value, 0x0012);
  EXPECT_EQ(pit_.channels[0].rw_byte, kPITByteMSB);

  // Write MSB
  PITWritePort(&pit_, kPITPortChannel0, 0x34);
  EXPECT_EQ(pit_.channels[0].reload_value, 0x3412);
  EXPECT_EQ(pit_.channels[0].rw_byte, kPITByteLSB);

  // --- Test Read ---
  // Set a known counter value internally for testing the latch.
  pit_.channels[0].counter = 0x5678;

  // Issue latch command for Channel 0.
  PITWritePort(&pit_, kPITPortControl, 0x00);
  EXPECT_TRUE(pit_.channels[0].latch_active);
  EXPECT_EQ(pit_.channels[0].latch, 0x5678);

  // Read LSB
  uint8_t lsb = PITReadPort(&pit_, kPITPortChannel0);
  EXPECT_EQ(lsb, 0x78);
  EXPECT_EQ(pit_.channels[0].rw_byte, kPITByteMSB);
  EXPECT_TRUE(pit_.channels[0].latch_active);  // Latch remains active.

  // Read MSB
  uint8_t msb = PITReadPort(&pit_, kPITPortChannel0);
  EXPECT_EQ(msb, 0x56);
  EXPECT_EQ(pit_.channels[0].rw_byte, kPITByteLSB);
  EXPECT_FALSE(pit_.channels[0].latch_active);  // Now latch is cleared.
}

}  // namespace
