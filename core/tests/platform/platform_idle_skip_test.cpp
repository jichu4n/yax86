#include <vector>

#include "gtest/gtest.h"
#include "platform.h"

namespace {

// Address at which test programs are loaded. Well clear of the interrupt
// vector table at 0x0000-0x03FF.
constexpr uint16_t kProgramOffset = 0x0100;
// Address of the handler every vector points at.
constexpr uint16_t kHandlerOffset = 0x0200;
// Address of the counter that handler increments, so that a test can see how
// often the guest went round its polling loop. Above the vector table, which
// the handler would otherwise be counting into.
constexpr uint16_t kCounterOffset = 0x1000;

// The DOS idle interrupt, and a vector that is not it.
constexpr uint8_t kDOSIdleVector = 0x28;
constexpr uint8_t kOtherVector = 0x29;

// What the SDL runtime gives the machine per display frame.
constexpr uint32_t kCyclesPerFrame = kCPUCyclesPerSecond / 60;

enum : uint8_t {
  kOpSti = 0xFB,
  // JMP rel8
  kOpJmpRel8 = 0xEB,
  // INT imm8
  kOpIntImm8 = 0xCD,
  kOpIret = 0xCF,
  // INC r/m16 (Group 5), with a ModR/M selecting REG 0 and a direct 16-bit
  // address - INC word [disp16].
  kOpGroup5Word = 0xFF,
  kModRMIncDirect = 0x06,
};

// PIT control word: channel 0, write LSB then MSB, mode 2 (rate generator).
constexpr uint8_t kChannel0Mode2 = 0x34;

// A machine whose only guest code is a loop that issues an interrupt and counts
// how often it has done so - the shape of MS-DOS waiting for a keystroke.
//
// A plain struct rather than a test fixture, because most of these tests run
// two machines side by side and compare them.
struct IdleMachine {
  PlatformConfig config = {0};
  PlatformState platform = {};
  uint8_t ram[64 * 1024] = {0};
  uint8_t vram[kCGAVRAMSize] = {0};

  // Returns false if the platform could not be initialized, which the caller
  // asserts on - a constructor cannot.
  bool Init(
      bool enable_idle_skip, uint8_t vector = kDOSIdleVector,
      bool program_timer = true) {
    config.physical_memory_size = sizeof(ram);
    config.physical_memory = ram;
    config.vram = vram;
    config.enable_dos_idle_skip = enable_idle_skip;
    if (!PlatformInit(&platform, &config)) {
      return false;
    }

    platform.cpu.registers[kCS] = 0;
    platform.cpu.registers[kIP] = kProgramOffset;
    platform.cpu.registers[kSS] = 0;
    platform.cpu.registers[kSP] = 0xFFFE;

    // Every vector points at the same handler, which counts the call and
    // returns. Without this the guest would vector through a zeroed table.
    for (uint32_t v = 0; v < 256; ++v) {
      ram[v * 4] = kHandlerOffset & 0xFF;
      ram[v * 4 + 1] = kHandlerOffset >> 8;
      ram[v * 4 + 2] = 0;
      ram[v * 4 + 3] = 0;
    }
    Write(
        kHandlerOffset, {kOpGroup5Word, kModRMIncDirect, kCounterOffset & 0xFF,
                         kCounterOffset >> 8, kOpIret});

    // STI, then issue the interrupt forever. The jump goes back to the INT
    // rather than the STI, so the loop is the two instructions DOS's own is.
    Write(
        kProgramOffset,
        {kOpSti, kOpIntImm8, vector, kOpJmpRel8, static_cast<uint8_t>(-4)});

    // Program the timer the way a BIOS does, at the full 65536 count that
    // gives 18.2Hz. A skip runs to the next device deadline, so how far it can
    // go depends on this - and an unprogrammed PIT has one due almost
    // immediately, which is not the machine any of this is about. Leaving it
    // unprogrammed instead schedules nothing at all, which is the case where
    // only the skip's own ceiling stops it.
    if (program_timer) {
      SetTimer(0);
    }
    return true;
  }

  void Write(uint16_t offset, const std::vector<uint8_t>& bytes) {
    for (size_t i = 0; i < bytes.size(); ++i) {
      ram[offset + i] = bytes[i];
    }
  }

  // Programs PIT channel 0 to produce an event reload_value ticks from now.
  void SetTimer(uint16_t reload_value) {
    WritePortByte(&platform, kPITPortControl, kChannel0Mode2);
    WritePortByte(&platform, kPITPortChannel0, reload_value & 0xFF);
    WritePortByte(&platform, kPITPortChannel0, reload_value >> 8);
  }

  // How many times the guest has been round its polling loop.
  uint16_t LoopCount() const {
    return static_cast<uint16_t>(
        ram[kCounterOffset] | (ram[kCounterOffset + 1] << 8));
  }

  // Runs one frame's worth of guest time and returns how much emulated time
  // that actually took, which should be the budget and not much more.
  uint32_t RunOneFrame() {
    const uint32_t start = platform.ticks;
    EXPECT_EQ(PlatformRun(&platform, kCyclesPerFrame), kPlatformRunning);
    return platform.ticks - start;
  }
};

// With the skip off the guest runs the whole budget, which is what every
// existing caller of PlatformRun depends on.
TEST(PlatformIdleSkipTest, DisabledRunsTheLoopForTheWholeBudget) {
  IdleMachine machine;
  ASSERT_TRUE(machine.Init(/*enable_idle_skip=*/false));

  const uint32_t elapsed = machine.RunOneFrame();

  EXPECT_GE(elapsed, kCyclesPerFrame);
  EXPECT_GT(machine.LoopCount(), 100u);
  EXPECT_FALSE(machine.platform.is_guest_idle);
}

// With it on the budget is still delivered in full - time is advanced, not
// discarded - but the guest barely runs to deliver it.
TEST(PlatformIdleSkipTest, IdleInterruptSkipsTheRestOfTheBudget) {
  IdleMachine machine;
  ASSERT_TRUE(machine.Init(/*enable_idle_skip=*/true));

  const uint32_t elapsed = machine.RunOneFrame();

  EXPECT_GE(elapsed, kCyclesPerFrame);
  // Never overrun the budget, which is what would let the guest's clock gain.
  EXPECT_LT(elapsed, kCyclesPerFrame * 2);
  // A pass or two round the loop rather than the hundreds the same frame costs
  // without the skip.
  EXPECT_LE(machine.LoopCount(), 2u);
}

// The saving is the point, so state it as a ratio rather than a bound.
TEST(PlatformIdleSkipTest, SkippingRetiresFarLessGuestWork) {
  IdleMachine without;
  ASSERT_TRUE(without.Init(/*enable_idle_skip=*/false));
  without.RunOneFrame();

  IdleMachine with;
  ASSERT_TRUE(with.Init(/*enable_idle_skip=*/true));
  with.RunOneFrame();

  ASSERT_GT(with.LoopCount(), 0u) << "the guest should still reach its loop";
  EXPECT_GT(without.LoopCount() / with.LoopCount(), 50u);
}

// Only the DOS idle interrupt means idle. Anything else is ordinary work and
// must not be skipped over.
TEST(PlatformIdleSkipTest, OtherInterruptsDoNotSkip) {
  IdleMachine machine;
  ASSERT_TRUE(machine.Init(/*enable_idle_skip=*/true, kOtherVector));

  const uint32_t elapsed = machine.RunOneFrame();

  EXPECT_GE(elapsed, kCyclesPerFrame);
  EXPECT_GT(machine.LoopCount(), 100u);
  EXPECT_FALSE(machine.platform.is_guest_idle);
}

// A skip stops at the next device deadline rather than running to the end of
// the budget, so a device with something to do still gets it done on time.
TEST(PlatformIdleSkipTest, SkipStopsAtTheNextDeviceDeadline) {
  IdleMachine machine;
  ASSERT_TRUE(machine.Init(/*enable_idle_skip=*/true));
  // Far enough out to be worth skipping to, well inside a frame.
  constexpr uint16_t kReloadTicks = 1000;
  machine.SetTimer(kReloadTicks);
  ASSERT_LT(kReloadTicks * kCyclesPerPITTick, kCyclesPerFrame / 4);

  machine.RunOneFrame();

  // Had the skip jumped the whole budget the guest would have gone round its
  // loop once; stopping at the timer instead leaves it several passes.
  EXPECT_GT(machine.LoopCount(), 2u);
}

// The property that makes the skip safe: it advances the clock rather than
// taking time away, so the same emulated time passes either way and the guest's
// sense of how long it has been idle does not change.
TEST(PlatformIdleSkipTest, EmulatedTimeIsUnchangedBySkipping) {
  constexpr int kFrames = 60;  // One second of emulated time.

  IdleMachine without;
  ASSERT_TRUE(without.Init(/*enable_idle_skip=*/false));
  IdleMachine with;
  ASSERT_TRUE(with.Init(/*enable_idle_skip=*/true));

  const uint32_t start_without = without.platform.ticks;
  const uint32_t start_with = with.platform.ticks;
  for (int i = 0; i < kFrames; ++i) {
    without.RunOneFrame();
    with.RunOneFrame();
  }
  const uint32_t elapsed_without = without.platform.ticks - start_without;
  const uint32_t elapsed_with = with.platform.ticks - start_with;

  // Both consumed the same budget. They differ only by how far the last
  // instruction of each frame overshot it, bounded by one instruction a frame.
  const uint32_t difference = elapsed_without > elapsed_with
                                  ? elapsed_without - elapsed_with
                                  : elapsed_with - elapsed_without;
  EXPECT_LT(difference, static_cast<uint32_t>(kFrames) * 100u);
}

// Devices are brought up to date across a skipped interval rather than having
// it taken away from them - which is what keeps the PIT, and so the guest's
// timer tick count, honest.
TEST(PlatformIdleSkipTest, DevicesAreSyncedAcrossASkip) {
  IdleMachine machine;
  ASSERT_TRUE(machine.Init(/*enable_idle_skip=*/true));

  machine.RunOneFrame();

  EXPECT_EQ(machine.platform.last_sync_ticks, machine.platform.ticks);
}

// A budget far larger than a display frame is delivered in bounded steps
// rather than in one jump. Nothing in the tree passes one - the SDL runtime
// passes a frame - but a skip is otherwise limited only by whatever the caller
// asked for, and the catch-up arithmetic in PlatformSync() is 32-bit.
//
// The PIT is left unprogrammed so that no device deadline is scheduled, which
// is the only case where the skip's own ceiling is what decides how far it
// goes. With the ceiling the budget takes many skips and the guest reaches its
// loop between each; without it, one skip swallows the lot.
TEST(PlatformIdleSkipTest, LargeBudgetIsDeliveredInBoundedSteps) {
  // A second of emulated time in one call, ten times the skip ceiling.
  constexpr uint32_t kBudget = kCPUCyclesPerSecond;

  IdleMachine machine;
  ASSERT_TRUE(machine.Init(
      /*enable_idle_skip=*/true, kDOSIdleVector, /*program_timer=*/false));
  ASSERT_EQ(PITTicksUntilNextEvent(&machine.platform.pit), kPITNoEvent)
      << "an unprogrammed PIT should schedule nothing";

  const uint32_t start = machine.platform.ticks;
  ASSERT_EQ(PlatformRun(&machine.platform, kBudget), kPlatformRunning);
  const uint32_t elapsed = machine.platform.ticks - start;

  // Delivered in full, and not overshot.
  EXPECT_GE(elapsed, kBudget);
  EXPECT_LT(elapsed - kBudget, kCyclesPerMillisecond);
  // And delivered in several capped steps rather than one unbounded one.
  EXPECT_GT(machine.LoopCount(), 5u);
}

}  // namespace
