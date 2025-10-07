
#include <gtest/gtest.h>

#include "platform.h"

// Frequency of the PIT tick in Hz.
static const int kPITTickFrequencyHz = 1193182;

// Last frequency set by the speaker callback.
static uint32_t g_last_speaker_frequency = 0;

// Mock callback to capture the speaker frequency.
static void SetPCSpeakerFrequency(void* context, uint32_t frequency_hz) {
  g_last_speaker_frequency = frequency_hz;
}

class PCSpeakerTest : public ::testing::Test {
 protected:
  PlatformState platform_;
  PlatformConfig platform_config_ = {0};

  void SetUp() override {
    // Reset the captured frequency before each test.
    g_last_speaker_frequency = 0;

    // Initialize platform config.
    platform_config_.physical_memory_size = 64 * 1024;
    platform_config_.pic_mode = kPlatformPICModeSingle;

    // Initialize the platform.
    ASSERT_TRUE(PlatformInit(&platform_, &platform_config_));

    // Wire up the mock speaker callback.
    // This is the connection that is currently a TODO in the main platform.
    platform_.ppi.config->set_pc_speaker_frequency = SetPCSpeakerFrequency;
  }

  // Helper to set the PIT frequency for channel 2.
  void SetPITFrequency(uint32_t freq_hz) {
    uint16_t reload_value = 0;
    if (freq_hz > 0) {
      reload_value = kPITTickFrequencyHz / freq_hz;
    }

    // Command to PIT: Channel 2, LSB then MSB, Mode 3 (Square Wave).
    WritePortByte(&platform_, kPITPortControl, 0b10110110);
    // Write reload value.
    WritePortByte(&platform_, kPITPortChannel2, reload_value & 0xFF);
    WritePortByte(&platform_, kPITPortChannel2, (reload_value >> 8) & 0xFF);
  }

  // Helper to enable the speaker via PPI Port B.
  void EnableSpeaker() {
    // Set bits 0 (Timer 2 Gate) and 1 (Speaker Data)
    WritePortByte(&platform_, kPPIPortB, 0b00000011);
  }

  // Helper to disable the speaker via PPI Port B.
  void DisableSpeaker() {
    // Clear bit 1 (Speaker Data)
    uint8_t port_b = ReadPortByte(&platform_, kPPIPortB);
    WritePortByte(&platform_, kPPIPortB, port_b & ~0b00000010);
  }

  // Helper to disable the speaker gate via PPI Port B.
  void DisableSpeakerGate() {
    // Clear bit 0 (Timer 2 Gate)
    uint8_t port_b = ReadPortByte(&platform_, kPPIPortB);
    WritePortByte(&platform_, kPPIPortB, port_b & ~0b00000001);
  }

  // Helper to get the actual frequency the PIT will generate for a given
  // target frequency, accounting for integer division.
  uint32_t GetExpectedFrequency(uint32_t target_freq_hz) {
    if (target_freq_hz == 0) {
      return 0;
    }
    uint16_t reload_value = kPITTickFrequencyHz / target_freq_hz;
    if (reload_value == 0) {
      return kPITTickFrequencyHz / 0x10000;
    }
    return kPITTickFrequencyHz / reload_value;
  }
};

TEST_F(PCSpeakerTest, SpeakerIsOffByDefault) {
  EXPECT_EQ(g_last_speaker_frequency, 0);
}

TEST_F(PCSpeakerTest, SetFrequencyThenEnableSpeaker) {
  SetPITFrequency(1000);
  // Setting frequency alone should not turn on the speaker.
  EXPECT_EQ(g_last_speaker_frequency, 0);

  EnableSpeaker();
  // Now the speaker should be on with the set frequency.
  EXPECT_EQ(g_last_speaker_frequency, GetExpectedFrequency(1000));
}

TEST_F(PCSpeakerTest, EnableSpeakerThenSetFrequency) {
  EnableSpeaker();
  // When speaker is enabled before PIT is programmed, frequency should be 0,
  // as the initial reload value is 0, which the PIT handles but the PPI's
  // initial state for the frequency is 0.
  EXPECT_EQ(g_last_speaker_frequency, 0);

  SetPITFrequency(2500);
  // Now the frequency should be updated.
  EXPECT_EQ(g_last_speaker_frequency, GetExpectedFrequency(2500));
}

TEST_F(PCSpeakerTest, DisableSpeaker) {
  SetPITFrequency(1234);
  EnableSpeaker();
  EXPECT_EQ(g_last_speaker_frequency, GetExpectedFrequency(1234));

  DisableSpeaker();
  // Disabling the speaker should set the frequency to 0.
  EXPECT_EQ(g_last_speaker_frequency, 0);
}

TEST_F(PCSpeakerTest, DisableSpeakerByGate) {
  SetPITFrequency(4321);
  EnableSpeaker();
  EXPECT_EQ(g_last_speaker_frequency, GetExpectedFrequency(4321));

  DisableSpeakerGate();
  // Disabling the timer gate should also set the frequency to 0.
  EXPECT_EQ(g_last_speaker_frequency, 0);
}

TEST_F(PCSpeakerTest, ChangingFrequencyWhileOn) {
  SetPITFrequency(1000);
  EnableSpeaker();
  EXPECT_EQ(g_last_speaker_frequency, GetExpectedFrequency(1000));

  SetPITFrequency(440);
  EXPECT_EQ(g_last_speaker_frequency, GetExpectedFrequency(440));

  SetPITFrequency(2000);
  EXPECT_EQ(g_last_speaker_frequency, GetExpectedFrequency(2000));
}
