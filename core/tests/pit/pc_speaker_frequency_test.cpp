#include <gtest/gtest.h>

#include <vector>

#include "pit.h"

namespace {

// Every frequency reported by the PIT, in order.
static std::vector<uint32_t> reported_frequencies;

static void MockSetSpeakerFrequency(void* context, uint32_t frequency_hz) {
  reported_frequencies.push_back(frequency_hz);
}

// Control word bits, laid out as SC1 SC0 RW1 RW0 M2 M1 M0 BCD.
enum {
  kSelectChannel2 = 0x80,
  kAccessLSBThenMSB = 0x30,
  kModeShift = 1,
};

// Builds a control word selecting channel 2 with LSB-then-MSB access.
static uint8_t Channel2ControlWord(uint8_t mode) {
  return kSelectChannel2 | kAccessLSBThenMSB | (mode << kModeShift);
}

class PCSpeakerFrequencyTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.context = nullptr;
    config_.set_pc_speaker_frequency = MockSetSpeakerFrequency;
    PITInit(&pit_, &config_);
    reported_frequencies.clear();
  }

  // Programs channel 2 for the given mode. This reports silence, since the
  // channel has no count yet.
  void ProgramChannel2(uint8_t mode) {
    PITWritePort(&pit_, kPITPortControl, Channel2ControlWord(mode));
  }

  // Writes a 16-bit reload value to channel 2.
  void WriteChannel2Count(uint16_t count) {
    PITWritePort(&pit_, kPITPortChannel2, count & 0xFF);
    PITWritePort(&pit_, kPITPortChannel2, count >> 8);
  }

  // The most recently reported frequency.
  uint32_t LastFrequency() const {
    return reported_frequencies.empty() ? 0 : reported_frequencies.back();
  }

  PITConfig config_ = {0};
  PITState pit_ = {0};
};

TEST_F(PCSpeakerFrequencyTest, Mode3ReportsReloadFrequency) {
  ProgramChannel2(3);
  // 1193182 / 1193 = 1000.15...
  WriteChannel2Count(1193);
  EXPECT_EQ(LastFrequency(), 1000u);
}

TEST_F(PCSpeakerFrequencyTest, Mode2ReportsReloadFrequency) {
  // Mode 2 oscillates at the reload frequency too, even though its output is a
  // narrow pulse rather than a square wave.
  ProgramChannel2(2);
  WriteChannel2Count(1193);
  EXPECT_EQ(LastFrequency(), 1000u);
}

TEST_F(PCSpeakerFrequencyTest, Mode0IsSilent) {
  // Mode 0 produces a single edge at terminal count, not a tone.
  ProgramChannel2(0);
  WriteChannel2Count(1193);
  EXPECT_EQ(LastFrequency(), 0u);
}

TEST_F(PCSpeakerFrequencyTest, ReprogrammingToASilentModeStopsTheTone) {
  ProgramChannel2(3);
  WriteChannel2Count(1193);
  ASSERT_EQ(LastFrequency(), 1000u);

  ProgramChannel2(0);
  EXPECT_EQ(LastFrequency(), 0u);
}

TEST_F(PCSpeakerFrequencyTest, ReprogrammingStopsTheToneUntilTheCountArrives) {
  ProgramChannel2(3);
  WriteChannel2Count(1193);
  ASSERT_EQ(LastFrequency(), 1000u);

  // The hardware does not start counting until the new count is written, so
  // the tone stops at the control word and resumes once it arrives.
  ProgramChannel2(3);
  EXPECT_EQ(LastFrequency(), 0u);

  WriteChannel2Count(2386);
  // 1193182 / 2386 = 500.07...
  EXPECT_EQ(LastFrequency(), 500u);
}

TEST_F(PCSpeakerFrequencyTest, LSBThenMSBReportsOnlyOnceTheCountIsComplete) {
  ProgramChannel2(3);
  reported_frequencies.clear();

  // A half-written count must not be reported as an intermediate frequency.
  PITWritePort(&pit_, kPITPortChannel2, 0xA9);
  EXPECT_TRUE(reported_frequencies.empty());

  PITWritePort(&pit_, kPITPortChannel2, 0x04);
  EXPECT_EQ(reported_frequencies, std::vector<uint32_t>{1000u});
}

TEST_F(PCSpeakerFrequencyTest, ZeroReloadValueMeansTheLongestPeriod) {
  // The hardware treats a reload value of 0 as 0x10000, which is the lowest
  // frequency the channel can produce - not silence.
  ProgramChannel2(3);
  WriteChannel2Count(0);
  // 1193182 / 65536 = 18.2...
  EXPECT_EQ(LastFrequency(), 18u);
}

TEST_F(PCSpeakerFrequencyTest, OtherChannelsAreNotReported) {
  // Channel 0 drives IRQ 0, not the speaker.
  PITWritePort(&pit_, kPITPortControl, 0x36);
  PITWritePort(&pit_, kPITPortChannel0, 0xA9);
  PITWritePort(&pit_, kPITPortChannel0, 0x04);
  EXPECT_TRUE(reported_frequencies.empty());
}

TEST_F(PCSpeakerFrequencyTest, LatchCommandDoesNotReport) {
  ProgramChannel2(3);
  WriteChannel2Count(1193);
  reported_frequencies.clear();

  // Latching the count to read it back does not change what the speaker does.
  PITWritePort(&pit_, kPITPortControl, kSelectChannel2);
  EXPECT_TRUE(reported_frequencies.empty());
}

TEST_F(PCSpeakerFrequencyTest, NoCallbackConfigured) {
  config_.set_pc_speaker_frequency = nullptr;
  ProgramChannel2(3);
  WriteChannel2Count(1193);
  EXPECT_TRUE(reported_frequencies.empty());
}

}  // namespace
