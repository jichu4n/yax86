#include <vector>

#include "gtest/gtest.h"
#include "platform.h"

namespace {

// PPI port B bits that gate the speaker. Both must be set for it to sound.
enum : uint8_t {
  kTimer2Gate = 1 << 0,
  kSpeakerData = 1 << 1,
};

// Control word selecting channel 2 with LSB-then-MSB access, in mode 3.
enum : uint8_t {
  kChannel2Mode3 = 0xB6,
};

// Reload value producing a ~1000 Hz tone, and the frequency it yields.
// 1193182 / 1193 = 1000.15...
enum {
  kReload1000Hz = 1193,
  kFrequency1000Hz = 1000,
  // 1193182 / 2386 = 500.07...
  kReload500Hz = 2386,
  kFrequency500Hz = 500,
};

// Exercises the speaker the way a guest does, through the platform's I/O port
// map, and checks what reaches the host callback in PlatformConfig.
class PlatformPCSpeakerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.physical_memory_size = sizeof(ram_);
    config_.context = this;
    config_.physical_memory = ram_;
    config_.set_pc_speaker_frequency = [](PlatformState* p,
                                          uint32_t frequency_hz) {
      auto* test = static_cast<PlatformPCSpeakerTest*>(p->config->context);
      test->frequencies_.push_back(frequency_hz);
    };

    ASSERT_TRUE(PlatformInit(&platform_, &config_));
  }

  // Programs PIT channel 2 for a square wave at the given reload value.
  void SetTone(uint16_t reload_value) {
    WritePortByte(&platform_, kPITPortControl, kChannel2Mode3);
    WritePortByte(&platform_, kPITPortChannel2, reload_value & 0xFF);
    WritePortByte(&platform_, kPITPortChannel2, reload_value >> 8);
  }

  // Writes PPI port B, which is where the speaker's two gating bits live.
  void WritePortB(uint8_t value) {
    WritePortByte(&platform_, kPPIPortB, value);
  }

  // The most recently reported frequency, or 0 if nothing was reported.
  uint32_t LastFrequency() const {
    return frequencies_.empty() ? 0 : frequencies_.back();
  }

  PlatformConfig config_ = {0};
  PlatformState platform_ = {0};
  uint8_t ram_[64 * 1024] = {0};
  // Every frequency reported to the host, in order.
  std::vector<uint32_t> frequencies_;
};

TEST_F(PlatformPCSpeakerTest, EnablingTheSpeakerReportsTheTone) {
  SetTone(kReload1000Hz);
  // The tone is programmed but the speaker is still off, so nothing sounds.
  EXPECT_EQ(LastFrequency(), 0u);

  WritePortB(kTimer2Gate | kSpeakerData);
  EXPECT_EQ(LastFrequency(), kFrequency1000Hz);
}

TEST_F(PlatformPCSpeakerTest, EnablingBeforeProgrammingReportsTheTone) {
  // The other order works too: the PPI remembers the last frequency the PIT
  // reported and applies it when the speaker is switched on.
  WritePortB(kTimer2Gate | kSpeakerData);
  EXPECT_EQ(LastFrequency(), 0u);

  SetTone(kReload1000Hz);
  EXPECT_EQ(LastFrequency(), kFrequency1000Hz);
}

TEST_F(PlatformPCSpeakerTest, ClearingSpeakerDataSilencesIt) {
  SetTone(kReload1000Hz);
  WritePortB(kTimer2Gate | kSpeakerData);
  ASSERT_EQ(LastFrequency(), kFrequency1000Hz);

  WritePortB(kTimer2Gate);
  EXPECT_EQ(LastFrequency(), 0u);
}

TEST_F(PlatformPCSpeakerTest, ClearingTheTimerGateSilencesIt) {
  SetTone(kReload1000Hz);
  WritePortB(kTimer2Gate | kSpeakerData);
  ASSERT_EQ(LastFrequency(), kFrequency1000Hz);

  WritePortB(kSpeakerData);
  EXPECT_EQ(LastFrequency(), 0u);
}

TEST_F(PlatformPCSpeakerTest, ClearingBothBitsAtOnceSilencesIt) {
  // This is how GLaBIOS ends a beep - it restores the port B value it saved
  // before the beep, clearing both bits in a single write.
  SetTone(kReload1000Hz);
  WritePortB(kTimer2Gate | kSpeakerData);
  ASSERT_EQ(LastFrequency(), kFrequency1000Hz);

  WritePortB(0);
  EXPECT_EQ(LastFrequency(), 0u);
}

TEST_F(PlatformPCSpeakerTest, ChangingTheToneWhileSounding) {
  SetTone(kReload1000Hz);
  WritePortB(kTimer2Gate | kSpeakerData);
  ASSERT_EQ(LastFrequency(), kFrequency1000Hz);

  SetTone(kReload500Hz);
  EXPECT_EQ(LastFrequency(), kFrequency500Hz);
}

TEST_F(PlatformPCSpeakerTest, NothingIsReportedWhileTheSpeakerIsOff) {
  SetTone(kReload1000Hz);
  SetTone(kReload500Hz);
  EXPECT_TRUE(frequencies_.empty());
}

TEST_F(PlatformPCSpeakerTest, NoCallbackConfigured) {
  // A host with no speaker leaves the callback NULL. Driving the speaker must
  // still work.
  config_.set_pc_speaker_frequency = nullptr;
  SetTone(kReload1000Hz);
  WritePortB(kTimer2Gate | kSpeakerData);
  WritePortB(0);
  EXPECT_TRUE(frequencies_.empty());
}

}  // namespace
