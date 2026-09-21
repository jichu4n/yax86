// PITAdvance() exists so that the platform can skip over the long uneventful
// stretches of a countdown instead of simulating every one of the 1.19 million
// ticks the PIT takes per second. That is only safe if it is
// indistinguishable from ticking one at a time, so these tests compare the two
// directly rather than asserting anything about counter values.
#include <gtest/gtest.h>

#include <vector>

#include "pit.h"

namespace {

// Records the IRQ 0 edges a PIT produces, so two PITs driven differently can be
// compared on what the rest of the machine would have seen.
struct Trace {
  std::vector<int> irq_ticks;
  int tick = 0;
};

static Trace* g_trace = nullptr;
static void MockRaiseIRQ0(void*) {
  if (g_trace) {
    g_trace->irq_ticks.push_back(g_trace->tick);
  }
}

class AdvanceTest : public ::testing::Test {
 protected:
  void InitPIT(PITState* pit, PITConfig* config) {
    config->context = nullptr;
    config->raise_irq_0 = MockRaiseIRQ0;
    config->set_pc_speaker_frequency = nullptr;
    PITInit(pit);
  }

  // Programs a channel with the given mode and reload value.
  void Program(PITState* pit, int channel, uint8_t mode, uint16_t reload) {
    const uint8_t control = static_cast<uint8_t>(
        (channel << 6) | (kPITAccessLSBThenMSB << 4) | (mode << 1));
    PITWritePort(pit, kPITPortControl, control);
    PITWritePort(
        pit, static_cast<uint16_t>(kPITPortChannel0 + channel),
        static_cast<uint8_t>(reload & 0xFF));
    PITWritePort(
        pit, static_cast<uint16_t>(kPITPortChannel0 + channel),
        static_cast<uint8_t>(reload >> 8));
  }

  // Runs the same program two ways - one tick at a time, and in a single
  // advance - and requires the resulting state and IRQ edges to match.
  void ExpectEquivalent(uint8_t mode, uint16_t reload, uint32_t ticks) {
    PITConfig stepped_config = {0};
    PITState stepped = {0};
    InitPIT(&stepped, &stepped_config);
    Program(&stepped, 0, mode, reload);

    PITConfig advanced_config = {0};
    PITState advanced = {0};
    InitPIT(&advanced, &advanced_config);
    Program(&advanced, 0, mode, reload);

    Trace stepped_trace;
    g_trace = &stepped_trace;
    for (uint32_t i = 0; i < ticks; ++i) {
      stepped_trace.tick = static_cast<int>(i);
      PITTick(&stepped);
    }

    Trace advanced_trace;
    g_trace = &advanced_trace;
    PITAdvance(&advanced, ticks);
    g_trace = nullptr;

    const std::string context = "mode " + std::to_string(mode) + ", reload " +
                                std::to_string(reload) + ", " +
                                std::to_string(ticks) + " ticks";
    EXPECT_EQ(advanced.channels[0].counter, stepped.channels[0].counter)
        << context;
    EXPECT_EQ(
        advanced.channels[0].output_state, stepped.channels[0].output_state)
        << context;
    // The tick numbers differ because an advance reports them all at once, so
    // only the number of edges is comparable.
    EXPECT_EQ(advanced_trace.irq_ticks.size(), stepped_trace.irq_ticks.size())
        << context;
  }

  // Writes a new reload value to a channel's data port without a control word
  // in front of it, which is how a guest changes a count on a channel it has
  // already programmed.
  void WriteReload(PITState* pit, int channel, uint16_t reload) {
    PITWritePort(
        pit, static_cast<uint16_t>(kPITPortChannel0 + channel),
        static_cast<uint8_t>(reload & 0xFF));
    PITWritePort(
        pit, static_cast<uint16_t>(kPITPortChannel0 + channel),
        static_cast<uint8_t>(reload >> 8));
  }

  // Channels 1 and 2 are advanced lazily: an advance charges them ticks, and
  // whatever looks at them next settles the count - skipping whole cycles on
  // the way, since nothing outside the chip saw the edges in between. Channel
  // 0 is walked tick by tick because its rising edge is IRQ 0, so programming
  // all three alike and comparing them compares the skipped answer against the
  // unskipped one.
  void ExpectDeferredChannelsMatchChannel0(
      uint8_t mode, uint16_t reload, uint32_t ticks) {
    PITConfig config = {0};
    PITState pit = {0};
    InitPIT(&pit, &config);
    for (int channel = 0; channel < kPITNumChannels; ++channel) {
      Program(&pit, channel, mode, reload);
    }
    PITAdvance(&pit, ticks);

    const std::string context = "mode " + std::to_string(mode) + ", reload " +
                                std::to_string(reload) + ", " +
                                std::to_string(ticks) + " ticks";
    for (int channel = 1; channel < kPITNumChannels; ++channel) {
      // Reading the channel's data port is one of the things that settles
      // what it owes.
      PITReadPort(&pit, static_cast<uint16_t>(kPITPortChannel0 + channel));
      const std::string where =
          context + ", channel " + std::to_string(channel);
      EXPECT_EQ(pit.channels[channel].pending_ticks, 0u) << where;
      EXPECT_EQ(pit.channels[channel].counter, pit.channels[0].counter)
          << where;
      EXPECT_EQ(
          pit.channels[channel].output_state, pit.channels[0].output_state)
          << where;
    }
  }
};

TEST_F(AdvanceTest, Mode0MatchesTicking) {
  for (uint16_t reload : {1, 2, 3, 17, 255, 1000}) {
    for (uint32_t ticks : {0u, 1u, 2u, 16u, 999u, 1001u, 5000u}) {
      ExpectEquivalent(0, reload, ticks);
    }
  }
}

TEST_F(AdvanceTest, Mode2MatchesTicking) {
  for (uint16_t reload : {2, 3, 4, 18, 256, 1193}) {
    for (uint32_t ticks : {0u, 1u, 2u, 17u, 1000u, 4096u}) {
      ExpectEquivalent(2, reload, ticks);
    }
  }
}

TEST_F(AdvanceTest, Mode3MatchesTicking) {
  // Both parities matter: an even count reaches terminal count at 0 and an odd
  // one wraps through 0xFFFF.
  for (uint16_t reload : {2, 3, 4, 5, 19, 256, 1193}) {
    for (uint32_t ticks : {0u, 1u, 2u, 17u, 1000u, 4096u}) {
      ExpectEquivalent(3, reload, ticks);
    }
  }
}

TEST_F(AdvanceTest, MatchesTickingAcrossTheSystemTimerPeriod) {
  // What the BIOS actually programs channel 0 with: mode 3 and a full 16-bit
  // count, giving the 18.2Hz system timer.
  ExpectEquivalent(3, 0, 200000);
}

TEST_F(AdvanceTest, TicksUntilNextEventNeverOvershoots) {
  // The scheduler relies on this being a lower bound - if it ever reported
  // more ticks than it takes to reach an edge, the platform would sleep
  // through that edge.
  for (uint8_t mode : {0, 2, 3}) {
    for (uint16_t reload : {1, 2, 3, 4, 5, 37, 4096}) {
      PITConfig config = {0};
      PITState pit = {0};
      InitPIT(&pit, &config);
      Program(&pit, 0, mode, reload);

      const uint32_t predicted = PITTicksUntilNextEvent(&pit);
      if (predicted == kPITNoEvent) {
        continue;
      }

      // Nothing may change before the predicted tick.
      const bool initial_output = pit.channels[0].output_state;
      for (uint32_t i = 0; i + 1 < predicted; ++i) {
        PITTick(&pit);
        ASSERT_EQ(pit.channels[0].output_state, initial_output)
            << "mode " << static_cast<int>(mode) << ", reload " << reload
            << " changed output at tick " << i << " but " << predicted
            << " was predicted";
      }
    }
  }
}

// Deadlines exist so that the platform knows when it must next look at the
// PIT. Only channel 0's output leaves the chip, so only channel 0 is worth
// being woken for - and asking for wake-ups on behalf of the other two is not
// free, because it truncates how far an idle machine may skip ahead.
TEST_F(AdvanceTest, OnlyChannelZeroSchedulesADeadline) {
  for (int channel = 1; channel < kPITNumChannels; ++channel) {
    PITConfig config = {0};
    PITState pit = {0};
    InitPIT(&pit, &config);
    // Mode 3 with a small count, which is what the BIOS leaves channel 2 in
    // after the POST beep, and would otherwise ask to be woken constantly.
    Program(&pit, channel, 3, 1356);

    EXPECT_EQ(PITTicksUntilNextEvent(&pit), kPITNoEvent)
        << "channel " << channel << " asked to be woken, but nothing outside "
        << "the PIT can observe its output";
  }
}

// The other half of the same invariant: channel 0 must still get one, or the
// system timer would be late.
TEST_F(AdvanceTest, ChannelZeroStillSchedulesADeadline) {
  PITConfig config = {0};
  PITState pit = {0};
  InitPIT(&pit, &config);
  Program(&pit, 0, 3, 1356);

  const uint32_t ticks = PITTicksUntilNextEvent(&pit);
  ASSERT_NE(ticks, kPITNoEvent);
  EXPECT_LE(ticks, 1356u);
}

// A channel nobody waits for still has to keep correct time, because a guest
// can read its counter back off its data port whenever it likes. Skipping its
// deadline, and charging it ticks rather than walking them, must not skip its
// bookkeeping.
TEST_F(AdvanceTest, UnscheduledChannelsStillAdvance) {
  // What the BIOS leaves channel 2 in after the POST beep.
  ExpectDeferredChannelsMatchChannel0(3, 1356, 5000);
}

TEST_F(AdvanceTest, DeferredChannelsMatchChannelZeroForEveryShapeOfCount) {
  for (uint8_t mode : {0, 2, 3}) {
    // Both parities matter in mode 3, and a reload of 1 makes a period of one
    // tick in mode 2, where a whole cycle is every cycle.
    for (uint16_t reload : {1, 2, 3, 4, 5, 18, 19, 1356, 0}) {
      for (uint32_t ticks :
           {0u, 1u, 2u, 17u, 18u, 19u, 1000u, 4096u, 200000u}) {
        ExpectDeferredChannelsMatchChannel0(mode, reload, ticks);
      }
    }
  }
}

// A reload arriving on the data port alone, with no control word in front of
// it to settle the channel first, must not leave ticks owed from before the
// write to be applied to the count it just loaded. Channel 0 is never charged
// ticks, so it is what the answer should look like.
//
// The new count is prime and does not divide the ticks owed, so that applying
// them late cannot land back on the right answer by arithmetic accident - a
// count of 100 against 1000 ticks does, and hides the bug entirely.
TEST_F(AdvanceTest, AReloadSettlesWhatTheChannelOwedFirst) {
  PITConfig config = {0};
  PITState pit = {0};
  InitPIT(&pit, &config);
  for (int channel = 0; channel < kPITNumChannels; ++channel) {
    Program(&pit, channel, 2, 18);
  }
  PITAdvance(&pit, 1000);

  for (int channel = 0; channel < kPITNumChannels; ++channel) {
    WriteReload(&pit, channel, 7);
  }

  for (int channel = 1; channel < kPITNumChannels; ++channel) {
    PITReadPort(&pit, static_cast<uint16_t>(kPITPortChannel0 + channel));
    EXPECT_EQ(pit.channels[channel].pending_ticks, 0u) << "channel " << channel;
    EXPECT_EQ(pit.channels[channel].counter, pit.channels[0].counter)
        << "channel " << channel;
    EXPECT_EQ(pit.channels[channel].counter, 7u) << "channel " << channel;
  }
}

// A latch takes the counter as it stands, not as it stood when something last
// looked at the channel.
TEST_F(AdvanceTest, ALatchSettlesWhatTheChannelOwedFirst) {
  PITConfig config = {0};
  PITState pit = {0};
  InitPIT(&pit, &config);
  for (int channel = 0; channel < kPITNumChannels; ++channel) {
    Program(&pit, channel, 2, 18);
  }
  PITAdvance(&pit, 1000);

  const uint8_t latch_channel_2 =
      static_cast<uint8_t>((2 << 6) | (kPITAccessLatch << 4));
  PITWritePort(&pit, kPITPortControl, latch_channel_2);

  EXPECT_EQ(pit.channels[2].pending_ticks, 0u);
  EXPECT_TRUE(pit.channels[2].latch_active);
  EXPECT_EQ(pit.channels[2].latch, pit.channels[0].counter);
}

}  // namespace
