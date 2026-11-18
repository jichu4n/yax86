#include <vector>

#include "gtest/gtest.h"
#include "platform.h"

namespace {

constexpr uint16_t kProgramOffset = 0x0100;
constexpr uint16_t kHandlerOffset = 0x0200;
constexpr uint16_t kCounterOffset = 0x1000;

enum : uint8_t {
  kOpNop = 0x90,
  kOpSti = 0xFB,
  kOpHlt = 0xF4,
  kOpJmpRel8 = 0xEB,
  kOpIntImm8 = 0xCD,
  kOpIret = 0xCF,
  kOpGroup5Word = 0xFF,
  kModRMIncDirect = 0x06,
};

// PIT control word: channel 0, write LSB then MSB, mode 2 (rate generator).
constexpr uint8_t kChannel0Mode2 = 0x34;
// PIT control word: channel 0, write LSB then MSB, mode 0 (interrupt on
// terminal count).
constexpr uint8_t kChannel0Mode0 = 0x30;

constexpr uint32_t kPITTicksPerFDCTick =
    Y_PLATFORM_CYCLES_PER_FDC_TICK / Y_PLATFORM_CYCLES_PER_PIT_TICK;
constexpr uint32_t kPITTicksPerMillisecond =
    Y_PLATFORM_CYCLES_PER_MILLISECOND / Y_PLATFORM_CYCLES_PER_PIT_TICK;
constexpr uint32_t kMaxIdleSkipPITTicks =
    (Y_PLATFORM_CPU_CYCLES_PER_SECOND / 10) / Y_PLATFORM_CYCLES_PER_PIT_TICK;

uint8_t MockImageRead(
    Y_UNUSED void* context, Y_UNUSED uint8_t drive, uint32_t offset) {
  return static_cast<uint8_t>(offset & 0xFF);
}

class PlatformCyclesPerPITTickTest : public ::testing::Test {
 protected:
  void SetUp() override { ResetMachine(); }

  void ResetMachine() {
    platform_ = YPlatformState{};
    for (size_t i = 0; i < sizeof(ram_); ++i) {
      ram_[i] = 0;
    }
    platform_.config.direct_memory_size = sizeof(ram_);
    platform_.config.context = this;
    platform_.config.direct_memory = ram_;
    platform_.config.vram = vram_;

    ASSERT_TRUE(YPlatformInit(&platform_));

    platform_.cpu.registers[Y_CPU_CS] = 0;
    platform_.cpu.registers[Y_CPU_IP] = kProgramOffset;
    platform_.cpu.registers[Y_CPU_SS] = 0;
    platform_.cpu.registers[Y_CPU_SP] = 0xFFFE;
  }

  // Advances the platform clock by exact_cycles and syncs devices.
  void AdvanceCycles(uint32_t exact_cycles) {
    platform_.ticks += exact_cycles;
    YPlatformSync(&platform_);
  }

  void SetTimerMode0(uint16_t count) {
    YPlatformWritePortByte(&platform_, Y_PIT_PORT_CONTROL, kChannel0Mode0);
    YPlatformWritePortByte(&platform_, Y_PIT_PORT_CHANNEL0, count & 0xFF);
    YPlatformWritePortByte(&platform_, Y_PIT_PORT_CHANNEL0, count >> 8);
  }

  void SetTimerMode2(uint16_t reload_value) {
    YPlatformWritePortByte(&platform_, Y_PIT_PORT_CONTROL, kChannel0Mode2);
    YPlatformWritePortByte(
        &platform_, Y_PIT_PORT_CHANNEL0, reload_value & 0xFF);
    YPlatformWritePortByte(&platform_, Y_PIT_PORT_CHANNEL0, reload_value >> 8);
  }

  YPlatformState platform_ = {};
  uint8_t ram_[64 * 1024] = {0};
  uint8_t vram_[Y_VIDEO_CGA_VRAM_SIZE] = {0};
};

TEST_F(PlatformCyclesPerPITTickTest, DefaultsToFourAndClampsBelowFour) {
  EXPECT_EQ(
      platform_.cycles_per_pit_tick,
      static_cast<uint32_t>(Y_PLATFORM_CYCLES_PER_PIT_TICK));
  EXPECT_EQ(platform_.pit_tick_shift, 2u);
  EXPECT_EQ(
      YPlatformGetCPUCyclesPerSecond(&platform_),
      static_cast<uint32_t>(Y_PLATFORM_CPU_CYCLES_PER_SECOND));

  for (uint32_t below : {0u, 1u, 2u, 3u}) {
    YPlatformSetCyclesPerPITTick(&platform_, below);
    EXPECT_EQ(
        platform_.cycles_per_pit_tick,
        static_cast<uint32_t>(Y_PLATFORM_CYCLES_PER_PIT_TICK));
    EXPECT_EQ(platform_.pit_tick_shift, 2u);
  }
}

TEST_F(
    PlatformCyclesPerPITTickTest,
    ClampsAboveMaxAndCapsDefaultEventIntervalAtUint16Max) {
  for (uint32_t above :
       {static_cast<uint32_t>(Y_PLATFORM_MAX_CYCLES_PER_PIT_TICK) + 1u, 1000u,
        0xFFFFFFFFu}) {
    YPlatformSetCyclesPerPITTick(&platform_, above);
    EXPECT_EQ(
        platform_.cycles_per_pit_tick,
        static_cast<uint32_t>(Y_PLATFORM_MAX_CYCLES_PER_PIT_TICK));
    EXPECT_EQ(platform_.pit_tick_shift, 8u);
    EXPECT_EQ(
        YPlatformGetCPUCyclesPerSecond(&platform_),
        static_cast<uint32_t>(
            static_cast<uint64_t>(Y_PLATFORM_CPU_CYCLES_PER_SECOND) *
            Y_PLATFORM_MAX_CYCLES_PER_PIT_TICK /
            Y_PLATFORM_CYCLES_PER_PIT_TICK));
    // At ratio 256, 1ms is 1,193 * 256 = 305,408 cycles, which exceeds
    // YCPUTick()'s uint16_t max_run_cycles; the default event deadline must be
    // capped at 0xFFFF.
    EXPECT_EQ(platform_.next_event_ticks - platform_.ticks, 0xFFFFu);
  }

  // At ratio 64 (16x), 1ms is 1,193 * 64 = 76,352 cycles, also capped at
  // 0xFFFF.
  YPlatformSetCyclesPerPITTick(&platform_, 64);
  EXPECT_EQ(platform_.cycles_per_pit_tick, 64u);
  EXPECT_EQ(platform_.pit_tick_shift, 6u);
  EXPECT_EQ(platform_.next_event_ticks - platform_.ticks, 0xFFFFu);
}

TEST_F(PlatformCyclesPerPITTickTest, TracksShiftAndCyclesPerSecondAt4And6And8) {
  YPlatformSetCyclesPerPITTick(&platform_, 6);
  EXPECT_EQ(platform_.cycles_per_pit_tick, 6u);
  EXPECT_EQ(platform_.pit_tick_shift, 0u);
  EXPECT_EQ(
      YPlatformGetCPUCyclesPerSecond(&platform_),
      static_cast<uint32_t>(
          static_cast<uint64_t>(Y_PLATFORM_CPU_CYCLES_PER_SECOND) * 6 / 4));

  YPlatformSetCyclesPerPITTick(&platform_, 8);
  EXPECT_EQ(platform_.cycles_per_pit_tick, 8u);
  EXPECT_EQ(platform_.pit_tick_shift, 3u);
  EXPECT_EQ(
      YPlatformGetCPUCyclesPerSecond(&platform_),
      static_cast<uint32_t>(Y_PLATFORM_CPU_CYCLES_PER_SECOND) * 2u);

  YPlatformSetCyclesPerPITTick(&platform_, 4);
  EXPECT_EQ(platform_.cycles_per_pit_tick, 4u);
  EXPECT_EQ(platform_.pit_tick_shift, 2u);
  EXPECT_EQ(
      YPlatformGetCPUCyclesPerSecond(&platform_),
      static_cast<uint32_t>(Y_PLATFORM_CPU_CYCLES_PER_SECOND));
}

TEST_F(PlatformCyclesPerPITTickTest, PITAdvancesAndFiresIRQ0At4And6And8) {
  for (uint32_t ratio : {4u, 6u, 8u}) {
    ResetMachine();
    YPlatformSetCyclesPerPITTick(&platform_, ratio);

    constexpr uint16_t kCount = 100;
    SetTimerMode0(kCount);
    platform_.pic.irr = 0;

    // One cycle short of kCount PIT ticks: counter is at 1, IRQ 0 has not
    // fired.
    AdvanceCycles(kCount * ratio - 1);
    EXPECT_EQ(platform_.pit.channels[0].counter, 1u) << "ratio=" << ratio;
    EXPECT_EQ(platform_.pit_cycles, ratio - 1) << "ratio=" << ratio;
    EXPECT_EQ(platform_.pic.irr & 1, 0) << "ratio=" << ratio;

    // The next cycle completes the 100th PIT tick and raises IRQ 0.
    AdvanceCycles(1);
    EXPECT_EQ(platform_.pit.channels[0].counter, 0u) << "ratio=" << ratio;
    EXPECT_EQ(platform_.pit_cycles, 0u) << "ratio=" << ratio;
    EXPECT_NE(platform_.pic.irr & 1, 0) << "ratio=" << ratio;
  }
}

TEST_F(
    PlatformCyclesPerPITTickTest,
    ProgramRunningUntilTimerInterruptRetiresWorkProportionalToRatio) {
  auto run_until_irq0 = [](uint32_t ratio) -> uint32_t {
    YPlatformState platform = {};
    static uint8_t ram[64 * 1024];
    static uint8_t vram[Y_VIDEO_CGA_VRAM_SIZE];
    for (size_t i = 0; i < sizeof(ram); ++i) {
      ram[i] = 0;
    }
    platform.config.direct_memory_size = sizeof(ram);
    platform.config.direct_memory = ram;
    platform.config.vram = vram;
    EXPECT_TRUE(YPlatformInit(&platform));
    YPlatformSetCyclesPerPITTick(&platform, ratio);

    // STI / loop: INC AX / JMP loop
    const uint8_t program[] = {
        kOpSti, 0x40, kOpJmpRel8, static_cast<uint8_t>(-3)};
    for (size_t i = 0; i < sizeof(program); ++i) {
      ram[kProgramOffset + i] = program[i];
    }
    // IRQ 0 handler: HLT
    ram[kHandlerOffset] = kOpHlt;
    ram[8 * 4] = kHandlerOffset & 0xFF;
    ram[8 * 4 + 1] = kHandlerOffset >> 8;

    YPlatformWritePortByte(&platform, 0x20, 0x13);
    YPlatformWritePortByte(&platform, 0x21, 0x08);
    YPlatformWritePortByte(&platform, 0x21, 0x01);
    YPlatformWritePortByte(&platform, 0x21, 0xFE);

    // Mode 0, 500 PIT ticks.
    constexpr uint16_t kTicks = 500;
    YPlatformWritePortByte(&platform, Y_PIT_PORT_CONTROL, kChannel0Mode0);
    YPlatformWritePortByte(&platform, Y_PIT_PORT_CHANNEL0, kTicks & 0xFF);
    YPlatformWritePortByte(&platform, Y_PIT_PORT_CHANNEL0, kTicks >> 8);

    platform.cpu.registers[Y_CPU_CS] = 0;
    platform.cpu.registers[Y_CPU_IP] = kProgramOffset;
    platform.cpu.registers[Y_CPU_SS] = 0;
    platform.cpu.registers[Y_CPU_SP] = 0xFFFE;

    while (!platform.cpu.is_halted) {
      YPlatformRun(&platform, 64);
    }
    return platform.cpu.registers[Y_CPU_AX];
  };

  const uint32_t loops_at_4 = run_until_irq0(4);
  const uint32_t loops_at_6 = run_until_irq0(6);
  const uint32_t loops_at_8 = run_until_irq0(8);

  ASSERT_GT(loops_at_4, 50u);
  EXPECT_GT(loops_at_6, loops_at_4 * 13 / 10);
  EXPECT_LT(loops_at_6, loops_at_4 * 17 / 10);
  EXPECT_GT(loops_at_8, loops_at_4 * 18 / 10);
  EXPECT_LT(loops_at_8, loops_at_4 * 22 / 10);
}

TEST_F(PlatformCyclesPerPITTickTest, FDCTicksScaleWithRatioAt4And6And8) {
  for (uint32_t ratio : {4u, 6u, 8u}) {
    ResetMachine();
    platform_.fdc.config.read_image_byte = MockImageRead;
    YPlatformSetCyclesPerPITTick(&platform_, ratio);

    // Enable FDC DMA/IRQ and clear reset sense interrupts.
    YPlatformWritePortByte(&platform_, 0x21, 0xBF);
    YPlatformWritePortByte(&platform_, 0x3F2, 0x00);
    YPlatformWritePortByte(&platform_, 0x3F2, 0x0C);
    YFDCTick(&platform_.fdc);
    for (int i = 0; i < 4; ++i) {
      YPlatformWritePortByte(&platform_, 0x3F5, 0x08);
      YFDCTick(&platform_.fdc);
      YPlatformReadPortByte(&platform_, 0x3F5);
      YPlatformReadPortByte(&platform_, 0x3F5);
    }

    // DMA channel 2 write, 512 bytes to 0x2000.
    YPlatformWritePortByte(&platform_, 0x0B, 0x46);
    YPlatformWritePortByte(&platform_, 0x0C, 0x00);
    YPlatformWritePortByte(&platform_, 0x04, 0x00);
    YPlatformWritePortByte(&platform_, 0x04, 0x20);
    YPlatformWritePortByte(&platform_, 0x81, 0x00);
    YPlatformWritePortByte(&platform_, 0x05, 0xFF);
    YPlatformWritePortByte(&platform_, 0x05, 0x01);
    YPlatformWritePortByte(&platform_, 0x0A, 0x02);

    YFDCInsertDisk(&platform_.fdc, 0, &YFDCFormat360KB);
    platform_.pic.irr = 0;

    // Issue Read Data for 1 sector (1 init tick + 512 byte ticks + 1 TC finish
    // tick = 514 FDC ticks).
    YPlatformWritePortByte(&platform_, 0x3F5, 0x06);
    YPlatformWritePortByte(&platform_, 0x3F5, 0x00);
    YPlatformWritePortByte(&platform_, 0x3F5, 0x00);
    YPlatformWritePortByte(&platform_, 0x3F5, 0x00);
    YPlatformWritePortByte(&platform_, 0x3F5, 0x01);
    YPlatformWritePortByte(&platform_, 0x3F5, 0x02);
    YPlatformWritePortByte(&platform_, 0x3F5, 0x09);
    YPlatformWritePortByte(&platform_, 0x3F5, 0x2A);
    YPlatformWritePortByte(&platform_, 0x3F5, 0xFF);
    ASSERT_EQ(platform_.fdc.phase, Y_FDC_PHASE_EXECUTION);

    const uint32_t cycles_per_fdc_tick = kPITTicksPerFDCTick * ratio;
    // The next event deadline while FDC is executing is at most one FDC tick.
    EXPECT_EQ(platform_.next_event_ticks - platform_.ticks, cycles_per_fdc_tick)
        << "ratio=" << ratio;

    // 513 FDC ticks leave the command one tick short of finishing.
    AdvanceCycles(513 * cycles_per_fdc_tick);
    EXPECT_EQ(platform_.fdc.phase, Y_FDC_PHASE_EXECUTION) << "ratio=" << ratio;
    EXPECT_EQ(platform_.pic.irr & (1 << 6), 0) << "ratio=" << ratio;

    // The 514th FDC tick finishes the sector and raises IRQ 6.
    AdvanceCycles(cycles_per_fdc_tick);
    EXPECT_EQ(platform_.fdc.phase, Y_FDC_PHASE_RESULT) << "ratio=" << ratio;
    EXPECT_NE(platform_.pic.irr & (1 << 6), 0) << "ratio=" << ratio;
  }
}

TEST_F(PlatformCyclesPerPITTickTest, KeyboardTicksScaleWithRatioAt4And6And8) {
  for (uint32_t ratio : {4u, 6u, 8u}) {
    ResetMachine();
    YPlatformSetCyclesPerPITTick(&platform_, ratio);

    // Hold keyboard clock low via PPI port B (bit 6 = 0), then release it
    // (bit 6 = 1). The keyboard requires 20ms of clock-low before queueing
    // 0xAA, and 1ms after release to send it to the PPI.
    YPlatformWritePortByte(&platform_, 0x61, 0x00);
    const uint32_t cycles_per_ms = kPITTicksPerMillisecond * ratio;

    AdvanceCycles(19 * cycles_per_ms);
    YPlatformWritePortByte(&platform_, 0x61, 0x40);
    AdvanceCycles(cycles_per_ms);
    EXPECT_EQ(YPlatformReadPortByte(&platform_, 0x60), 0x00)
        << "19ms should not complete reset at ratio=" << ratio;

    YPlatformWritePortByte(&platform_, 0x61, 0x00);
    AdvanceCycles(20 * cycles_per_ms);
    YPlatformWritePortByte(&platform_, 0x61, 0x40);
    AdvanceCycles(cycles_per_ms);
    EXPECT_EQ(YPlatformReadPortByte(&platform_, 0x60), 0xAA)
        << "20ms should complete reset at ratio=" << ratio;
  }
}

TEST_F(PlatformCyclesPerPITTickTest, VideoTimingScalesWithRatioAt4And6And8) {
  // CGA: 304 4.77MHz cycles (76 PIT ticks) per scan line, 200 displayed scan
  // lines before vertical retrace, 262 scan lines per frame.
  constexpr uint32_t kPITTicksPerScanLine =
      304 / Y_PLATFORM_CYCLES_PER_PIT_TICK;
  constexpr uint32_t kDisplayedScanLines = 200;
  constexpr uint32_t kScanLinesPerFrame = 262;

  for (uint32_t ratio : {4u, 6u, 8u}) {
    platform_ = YPlatformState{};
    platform_.config.direct_memory_size = sizeof(ram_);
    platform_.config.direct_memory = ram_;
    platform_.config.vram = vram_;
    platform_.config.video_adapter = Y_VIDEO_ADAPTER_CGA;
    ASSERT_TRUE(YPlatformInit(&platform_));
    YPlatformSetCyclesPerPITTick(&platform_, ratio);

    // One scan line short of vertical retrace (scan line 199): vertical retrace
    // is still clear.
    AdvanceCycles((kDisplayedScanLines - 1) * kPITTicksPerScanLine * ratio);
    EXPECT_EQ(platform_.video.scan_line, 199u) << "ratio=" << ratio;
    EXPECT_EQ(
        YPlatformReadPortByte(&platform_, Y_VIDEO_CGA_PORT_STATUS) &
            Y_VIDEO_STATUS_VERTICAL_RETRACE,
        0)
        << "ratio=" << ratio;

    // One more scan line reaches scan line 200 and enters vertical retrace.
    AdvanceCycles(kPITTicksPerScanLine * ratio);
    EXPECT_EQ(platform_.video.scan_line, 200u) << "ratio=" << ratio;
    EXPECT_NE(
        YPlatformReadPortByte(&platform_, Y_VIDEO_CGA_PORT_STATUS) &
            Y_VIDEO_STATUS_VERTICAL_RETRACE,
        0)
        << "ratio=" << ratio;
    EXPECT_EQ(platform_.video.num_frames, 0u) << "ratio=" << ratio;

    // The remaining 62 scan lines complete the first frame and return to scan
    // line 0.
    AdvanceCycles(
        (kScanLinesPerFrame - kDisplayedScanLines) * kPITTicksPerScanLine *
        ratio);
    EXPECT_EQ(platform_.video.scan_line, 0u) << "ratio=" << ratio;
    EXPECT_EQ(platform_.video.num_frames, 1u) << "ratio=" << ratio;
    EXPECT_EQ(
        YPlatformReadPortByte(&platform_, Y_VIDEO_CGA_PORT_STATUS) &
            Y_VIDEO_STATUS_VERTICAL_RETRACE,
        0)
        << "ratio=" << ratio;
  }
}

TEST_F(
    PlatformCyclesPerPITTickTest,
    EventIntervalAndIdleSkipCapScaleWithRatioAt4And6And8) {
  for (uint32_t ratio : {4u, 6u, 8u}) {
    ResetMachine();
    YPlatformSetCyclesPerPITTick(&platform_, ratio);
    ASSERT_EQ(YPITTicksUntilNextEvent(&platform_.pit), Y_PIT_NO_EVENT);

    // With no PIT or FDC event scheduled, the event interval is 1ms in scaled
    // cycles.
    YPlatformSync(&platform_);
    EXPECT_EQ(
        platform_.next_event_ticks - platform_.ticks,
        kPITTicksPerMillisecond * ratio)
        << "ratio=" << ratio;

    // Enable DOS idle skip and execute INT 28h with a budget larger than the
    // 100ms idle-skip cap.
    YPlatformSetDOSIdleSkip(&platform_, true);
    for (uint32_t v = 0; v < 256; ++v) {
      ram_[v * 4] = kHandlerOffset & 0xFF;
      ram_[v * 4 + 1] = kHandlerOffset >> 8;
    }
    ram_[kHandlerOffset] = kOpIret;
    const uint8_t code[] = {kOpIntImm8, 0x28, kOpHlt};
    for (size_t i = 0; i < sizeof(code); ++i) {
      ram_[kProgramOffset + i] = code[i];
    }

    const uint32_t max_skip = kMaxIdleSkipPITTicks * ratio;
    // Run with a budget of 2 * max_skip; the single INT 28h skip is capped at
    // max_skip before the guest hits HLT (with IF=0, returning HUNG).
    ASSERT_EQ(YPlatformRun(&platform_, max_skip * 2), Y_PLATFORM_HUNG);
    EXPECT_GE(platform_.ticks, max_skip) << "ratio=" << ratio;
    EXPECT_LT(platform_.ticks, max_skip + 1000u) << "ratio=" << ratio;
  }
}

TEST_F(
    PlatformCyclesPerPITTickTest,
    SetterSyncsElapsedCyclesUnderOldRatioBeforeSwitching) {
  YPlatformSetCyclesPerPITTick(&platform_, 8);
  SetTimerMode0(100);

  // 16 unsynced cycles under ratio 8 is 2 PIT ticks (not 16 / 4 = 4).
  platform_.ticks += 16;
  YPlatformSetCyclesPerPITTick(&platform_, 4);

  EXPECT_EQ(platform_.last_sync_ticks, platform_.ticks);
  EXPECT_EQ(platform_.pit.channels[0].counter, 98u);
  EXPECT_EQ(platform_.pit_cycles, 0u);
}

TEST_F(
    PlatformCyclesPerPITTickTest,
    LoweringRatioClampsPendingRemaindersAndDoesNotWrapDeadline) {
  YPlatformSetCyclesPerPITTick(&platform_, 8);
  // Schedule a PIT event 1 PIT tick from now (at ratio 8, 8 cycles away).
  SetTimerMode0(1);
  ASSERT_EQ(YPITTicksUntilNextEvent(&platform_.pit), 1u);

  // Put FDC into execution phase as well so both PIT and FDC deadlines are
  // active.
  platform_.fdc.phase = Y_FDC_PHASE_EXECUTION;

  // Advance 6 cycles: at ratio 8, neither the 8-cycle PIT tick nor the
  // 40-cycle FDC tick has fired, leaving remainders of 6.
  AdvanceCycles(6);
  ASSERT_EQ(platform_.pit.channels[0].counter, 1u);
  ASSERT_EQ(platform_.pit_cycles, 6u);
  ASSERT_EQ(platform_.fdc_cycles, 6u);
  ASSERT_EQ(platform_.keyboard_cycles, 6u);

  // Also test an FDC remainder between 20 and 39 and a keyboard remainder
  // between 4772 and 9543 by setting them before lowering the ratio from 8 to
  // 4.
  platform_.fdc_cycles = 35;
  platform_.keyboard_cycles = 7000;

  // Lower ratio to 4. Without clamping, pit_ticks * 4 - pit_cycles would be
  // 1 * 4 - 6 = 0xFFFFFFFE (wrapped) and the deadline would be missed.
  YPlatformSetCyclesPerPITTick(&platform_, 4);

  EXPECT_EQ(platform_.pit_cycles, 3u);
  EXPECT_EQ(platform_.fdc_cycles, 19u);
  EXPECT_EQ(
      platform_.keyboard_cycles,
      static_cast<uint32_t>(Y_PLATFORM_CYCLES_PER_MILLISECOND - 1));
  EXPECT_EQ(platform_.next_event_ticks - platform_.ticks, 1u);

  // Advancing 1 more cycle reaches both the PIT and FDC ticks.
  platform_.pic.irr = 0;
  AdvanceCycles(1);
  EXPECT_EQ(platform_.pit.channels[0].counter, 0u);
  EXPECT_NE(platform_.pic.irr & 1, 0);
  EXPECT_EQ(platform_.pit_cycles, 0u);
  EXPECT_EQ(platform_.fdc_cycles, 0u);
}

}  // namespace
