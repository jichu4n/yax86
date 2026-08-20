#include <vector>

#include "gtest/gtest.h"
#include "platform.h"

namespace {

// Address at which test programs are loaded. Well clear of the interrupt
// vector table at 0x0000-0x03FF.
constexpr uint16_t kProgramOffset = 0x0100;
// Address used by tests that watch a data access. Well clear of the program,
// so that instruction fetches do not trip the watchpoint.
constexpr uint16_t kDataOffset = 0x2000;

// Opcodes used to hand-assemble test programs.
enum : uint8_t {
  kOpNop = 0x90,
  kOpCli = 0xFA,
  kOpSti = 0xFB,
  kOpHlt = 0xF4,
  // MOV AL, imm8
  kOpMovAlImm8 = 0xB0,
  // MOV moffs8, AL
  kOpMovMoffs8Al = 0xA2,
  // MOV AL, moffs8
  kOpMovAlMoffs8 = 0xA0,
  // INC r/m8 (Group 4). Every single opcode byte now decodes to something, so
  // the only encodings this emulator rejects are undocumented REG values
  // within a group - here 0xFE with REG 2, i.e. a ModRM byte of 0xD0.
  kOpGroup4 = 0xFE,
  kModRMGroup4Reg2 = 0xD0,
};

class PlatformExecutionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    config_.physical_memory_size = sizeof(ram_);
    config_.context = this;
    config_.physical_memory = ram_;
    config_.vram = vram_;

    ASSERT_TRUE(PlatformInit(&platform_, &config_));

    // Run test programs out of RAM rather than the BIOS entry point.
    platform_.cpu.registers[kCS] = 0;
    platform_.cpu.registers[kIP] = kProgramOffset;
    platform_.cpu.registers[kSS] = 0;
    platform_.cpu.registers[kSP] = 0xFFFE;
  }

  void Load(const std::vector<uint8_t>& code) {
    for (size_t i = 0; i < code.size(); ++i) {
      ram_[kProgramOffset + i] = code[i];
    }
  }

  uint16_t ip() const { return platform_.cpu.registers[kIP]; }

  // Runs up to num_instructions instructions, stopping early if the platform
  // does. PlatformRun takes a budget in CPU cycles, but what most of these
  // tests are pinning down is execution control - how many instructions ran,
  // and what stopped them - so they count instructions instead and stay
  // independent of what any one of them costs.
  PlatformRunStatus RunInstructions(int num_instructions) {
    PlatformRunStatus status = kPlatformRunning;
    for (int i = 0; i < num_instructions; ++i) {
      status = PlatformTick(&platform_);
      if (status != kPlatformRunning) {
        return status;
      }
    }
    return status;
  }

  PlatformConfig config_ = {0};
  PlatformState platform_ = {};
  uint8_t ram_[64 * 1024] = {0};
  uint8_t vram_[kCGAVRAMSize] = {0};
};

TEST_F(PlatformExecutionTest, TickReportsRunning) {
  Load({kOpNop, kOpNop});

  EXPECT_EQ(PlatformTick(&platform_), kPlatformRunning);
  EXPECT_EQ(ip(), kProgramOffset + 1);
  EXPECT_EQ(PlatformGetStopInfo(&platform_), nullptr);
}

TEST_F(PlatformExecutionTest, RunConsumesFullBudget) {
  Load({kOpNop, kOpNop, kOpNop, kOpNop});

  EXPECT_EQ(RunInstructions(4), kPlatformRunning);
  EXPECT_EQ(ip(), kProgramOffset + 4);
}

TEST_F(PlatformExecutionTest, UnimplementedEncodingIsReportedAsInvalid) {
  Load({kOpNop, kOpGroup4, kModRMGroup4Reg2, kOpNop});

  EXPECT_EQ(RunInstructions(8), kPlatformInvalid);
  // IP has advanced past the offending instruction, so the caller can choose
  // to keep going.
  EXPECT_EQ(ip(), kProgramOffset + 3);
}

TEST_F(PlatformExecutionTest, RunResumesAfterInvalidInstruction) {
  Load({kOpNop, kOpGroup4, kModRMGroup4Reg2, kOpNop, kOpNop});

  ASSERT_EQ(RunInstructions(4), kPlatformInvalid);
  // The offending tick is counted, so a host that treats an invalid
  // instruction as non-fatal can resume and make progress.
  const uint32_t ticks_before = platform_.ticks;
  EXPECT_EQ(RunInstructions(2), kPlatformRunning);
  EXPECT_GT(platform_.ticks, ticks_before);
  EXPECT_EQ(ip(), kProgramOffset + 5);
}

TEST_F(PlatformExecutionTest, HaltWithInterruptsDisabledIsReportedAsHung) {
  Load({kOpCli, kOpHlt});

  EXPECT_EQ(RunInstructions(8), kPlatformHung);
  EXPECT_TRUE(platform_.cpu.is_halted);
}

TEST_F(PlatformExecutionTest, HaltWithInterruptsEnabledKeepsRunning) {
  Load({kOpSti, kOpHlt});

  // A halted CPU that can still be woken is not a stop: the rest of the
  // machine has to keep ticking so that an interrupt can arrive.
  EXPECT_EQ(PlatformRun(&platform_, 64), kPlatformRunning);
  EXPECT_TRUE(platform_.cpu.is_halted);
  // Time keeps passing even though the CPU is halted, which is what lets the
  // timer that would wake it keep running. A halted tick is charged a fixed
  // cost, so the run stops on the first one to reach the budget.
  EXPECT_GE(platform_.ticks, 64u);
  // A halted tick costs only a handful of cycles, so the run stops within one
  // of them of the budget rather than far past it.
  EXPECT_LT(platform_.ticks, 80u);
}

TEST_F(PlatformExecutionTest, BreakpointStopsBeforeExecutingInstruction) {
  Load({kOpNop, kOpMovAlImm8, 0x42, kOpNop});

  const int8_t index = PlatformAddBreakpoint(&platform_, 0, kProgramOffset + 1);
  ASSERT_GE(index, 0);

  EXPECT_EQ(RunInstructions(8), kPlatformStopped);
  EXPECT_EQ(ip(), kProgramOffset + 1);
  // The MOV has not run yet.
  EXPECT_EQ(platform_.cpu.registers[kAX] & 0xFF, 0);

  const PlatformStopInfo* stop_info = PlatformGetStopInfo(&platform_);
  ASSERT_NE(stop_info, nullptr);
  EXPECT_EQ(stop_info->reason, kPlatformStopBreakpoint);
  EXPECT_EQ(stop_info->index, index);
  EXPECT_EQ(stop_info->cs, 0);
  EXPECT_EQ(stop_info->ip, kProgramOffset + 1);
}

TEST_F(PlatformExecutionTest, ResumingFromBreakpointMakesProgress) {
  Load({kOpNop, kOpMovAlImm8, 0x42, kOpNop});
  ASSERT_GE(PlatformAddBreakpoint(&platform_, 0, kProgramOffset + 1), 0);
  ASSERT_EQ(RunInstructions(8), kPlatformStopped);

  // Resuming must execute the instruction under the breakpoint rather than
  // stopping on it again. Two ticks run the MOV and the trailing NOP.
  EXPECT_EQ(RunInstructions(2), kPlatformRunning);
  EXPECT_EQ(platform_.cpu.registers[kAX] & 0xFF, 0x42);
  EXPECT_EQ(ip(), kProgramOffset + 4);
}

TEST_F(PlatformExecutionTest, RemovedBreakpointDoesNotFire) {
  Load({kOpNop, kOpNop, kOpNop, kOpNop});

  const int8_t index = PlatformAddBreakpoint(&platform_, 0, kProgramOffset + 2);
  ASSERT_GE(index, 0);
  ASSERT_TRUE(PlatformRemoveBreakpoint(&platform_, index));
  // Removing it a second time reports failure.
  EXPECT_FALSE(PlatformRemoveBreakpoint(&platform_, index));

  EXPECT_EQ(RunInstructions(4), kPlatformRunning);
  EXPECT_EQ(ip(), kProgramOffset + 4);
}

TEST_F(PlatformExecutionTest, AddBreakpointFailsWhenFull) {
  for (int i = 0; i < kMaxBreakpoints; ++i) {
    EXPECT_EQ(PlatformAddBreakpoint(&platform_, 0, (uint16_t)(0x8000 + i)), i);
  }
  EXPECT_EQ(PlatformAddBreakpoint(&platform_, 0, 0x9000), kInvalidWatchIndex);

  PlatformClearBreakpoints(&platform_);
  EXPECT_EQ(PlatformAddBreakpoint(&platform_, 0, 0x9000), 0);
}

TEST_F(PlatformExecutionTest, MemoryWatchpointStopsOnWrite) {
  // MOV AL, 0x42 / MOV [kDataOffset], AL / NOP
  Load(
      {kOpMovAlImm8, 0x42, kOpMovMoffs8Al, kDataOffset & 0xFF, kDataOffset >> 8,
       kOpNop});

  const int8_t index = PlatformAddMemoryWatchpoint(
      &platform_, kDataOffset, kDataOffset, /*on_read=*/false,
      /*on_write=*/true);
  ASSERT_GE(index, 0);

  EXPECT_EQ(RunInstructions(8), kPlatformStopped);
  // The instruction that tripped the watchpoint runs to completion.
  EXPECT_EQ(ram_[kDataOffset], 0x42);

  const PlatformStopInfo* stop_info = PlatformGetStopInfo(&platform_);
  ASSERT_NE(stop_info, nullptr);
  EXPECT_EQ(stop_info->reason, kPlatformStopMemoryWatchpoint);
  EXPECT_EQ(stop_info->index, index);
  EXPECT_EQ(stop_info->address, kDataOffset);
  EXPECT_TRUE(stop_info->is_write);
}

TEST_F(PlatformExecutionTest, MemoryWatchpointIgnoresUnwatchedDirection) {
  // MOV AL, [kDataOffset] / NOP
  Load({kOpMovAlMoffs8, kDataOffset & 0xFF, kDataOffset >> 8, kOpNop, kOpNop});
  ram_[kDataOffset] = 0x99;

  // Watch writes only - the program only reads.
  ASSERT_GE(
      PlatformAddMemoryWatchpoint(
          &platform_, kDataOffset, kDataOffset, /*on_read=*/false,
          /*on_write=*/true),
      0);

  EXPECT_EQ(RunInstructions(3), kPlatformRunning);
  EXPECT_EQ(platform_.cpu.registers[kAX] & 0xFF, 0x99);
  EXPECT_EQ(PlatformGetStopInfo(&platform_), nullptr);
}

TEST_F(PlatformExecutionTest, MemoryWatchpointStopsOnRead) {
  Load({kOpMovAlMoffs8, kDataOffset & 0xFF, kDataOffset >> 8, kOpNop, kOpNop});
  ram_[kDataOffset] = 0x99;

  ASSERT_GE(
      PlatformAddMemoryWatchpoint(
          &platform_, kDataOffset, kDataOffset, /*on_read=*/true,
          /*on_write=*/false),
      0);

  EXPECT_EQ(RunInstructions(3), kPlatformStopped);

  const PlatformStopInfo* stop_info = PlatformGetStopInfo(&platform_);
  ASSERT_NE(stop_info, nullptr);
  EXPECT_EQ(stop_info->reason, kPlatformStopMemoryWatchpoint);
  EXPECT_EQ(stop_info->address, kDataOffset);
  EXPECT_FALSE(stop_info->is_write);
}

TEST_F(PlatformExecutionTest, ClearedMemoryWatchpointDoesNotFire) {
  Load({kOpMovAlMoffs8, kDataOffset & 0xFF, kDataOffset >> 8, kOpNop, kOpNop});

  ASSERT_GE(
      PlatformAddMemoryWatchpoint(
          &platform_, kDataOffset, kDataOffset, /*on_read=*/true,
          /*on_write=*/true),
      0);
  PlatformClearMemoryWatchpoints(&platform_);

  EXPECT_EQ(RunInstructions(3), kPlatformRunning);
}

TEST_F(PlatformExecutionTest, AddMemoryWatchpointRejectsInvalidRange) {
  EXPECT_EQ(
      PlatformAddMemoryWatchpoint(&platform_, 0x200, 0x100, true, true),
      kInvalidWatchIndex);
  // Watching neither reads nor writes would never fire.
  EXPECT_EQ(
      PlatformAddMemoryWatchpoint(&platform_, 0x100, 0x200, false, false),
      kInvalidWatchIndex);
}

TEST_F(PlatformExecutionTest, StepModeStopsAfterEachInstruction) {
  Load({kOpNop, kOpNop, kOpNop});
  PlatformSetStepMode(&platform_, true);

  for (int i = 1; i <= 3; ++i) {
    EXPECT_EQ(RunInstructions(100), kPlatformStopped) << "step " << i;
    EXPECT_EQ(ip(), kProgramOffset + i);
    const PlatformStopInfo* stop_info = PlatformGetStopInfo(&platform_);
    ASSERT_NE(stop_info, nullptr);
    EXPECT_EQ(stop_info->reason, kPlatformStopStep);
    EXPECT_EQ(stop_info->ip, kProgramOffset + i);
  }

  PlatformSetStepMode(&platform_, false);
  EXPECT_EQ(RunInstructions(4), kPlatformRunning);
}

TEST_F(PlatformExecutionTest, StepModeStopsOnHaltingInstruction) {
  // STI first, so that the halt is wakeable and does not report as hung.
  Load({kOpSti, kOpHlt, kOpNop});
  PlatformSetStepMode(&platform_, true);

  EXPECT_EQ(RunInstructions(100), kPlatformStopped);
  EXPECT_EQ(ip(), kProgramOffset + 1);

  // HLT is an instruction, so stepping must stop after it runs rather than
  // running away because the CPU happens to be halted afterwards.
  EXPECT_EQ(RunInstructions(100), kPlatformStopped);
  EXPECT_EQ(ip(), kProgramOffset + 2);
  const PlatformStopInfo* stop_info = PlatformGetStopInfo(&platform_);
  ASSERT_NE(stop_info, nullptr);
  EXPECT_EQ(stop_info->reason, kPlatformStopStep);
  EXPECT_TRUE(platform_.cpu.is_halted);

  // Once halted, no instruction retires, so stepping does not stop again -
  // the machine simply keeps ticking until an interrupt wakes the CPU.
  EXPECT_EQ(RunInstructions(100), kPlatformRunning);
  EXPECT_TRUE(platform_.cpu.is_halted);
}

TEST_F(PlatformExecutionTest, HungIsReportedAheadOfAStepStop) {
  Load({kOpCli, kOpHlt});
  PlatformSetStepMode(&platform_, true);

  EXPECT_EQ(RunInstructions(100), kPlatformStopped);

  // The HLT retires, which would otherwise be a step stop, but a CPU that can
  // never be woken is the more useful thing to report.
  EXPECT_EQ(RunInstructions(100), kPlatformHung);
}

// The platform counts retired instructions so that a caller does not have to
// drive it one instruction at a time to find out - which is what a benchmark
// harness would otherwise do, giving up PlatformRun()'s batching for a number
// the platform already has.
TEST_F(PlatformExecutionTest, CountsRetiredInstructions) {
  Load({kOpNop, kOpNop, kOpNop, kOpNop});

  EXPECT_EQ(platform_.cpu.instructions_retired, 0u);
  ASSERT_EQ(RunInstructions(4), kPlatformRunning);
  EXPECT_EQ(platform_.cpu.instructions_retired, 4u);
}

// A halted CPU retires nothing, however long the machine is left running. The
// clock still advances, which is what lets an interrupt arrive and wake it.
TEST_F(PlatformExecutionTest, HaltedTicksRetireNoInstructions) {
  Load({kOpSti, kOpHlt});
  ASSERT_EQ(RunInstructions(2), kPlatformRunning);
  const uint64_t retired_at_halt = platform_.cpu.instructions_retired;
  const uint32_t ticks_at_halt = platform_.ticks;

  ASSERT_EQ(RunInstructions(100), kPlatformRunning);

  EXPECT_EQ(platform_.cpu.instructions_retired, retired_at_halt);
  EXPECT_GT(platform_.ticks, ticks_at_halt);
}

// PlatformRun() must agree with PlatformTick() about what an instruction is,
// or the count would depend on how the caller chose to drive the machine.
TEST_F(PlatformExecutionTest, BatchedRunCountsTheSameInstructions) {
  Load({kOpNop, kOpNop, kOpNop, kOpNop, kOpNop, kOpNop, kOpNop, kOpNop});
  ASSERT_EQ(RunInstructions(8), kPlatformRunning);
  const uint64_t stepped = platform_.cpu.instructions_retired;

  PlatformConfig batched_config = {0};
  PlatformState batched = {};
  static uint8_t batched_ram[64 * 1024] = {0};
  static uint8_t batched_vram[kCGAVRAMSize] = {0};
  batched_config.physical_memory_size = sizeof(batched_ram);
  batched_config.physical_memory = batched_ram;
  batched_config.vram = batched_vram;
  ASSERT_TRUE(PlatformInit(&batched, &batched_config));
  batched.cpu.registers[kCS] = 0;
  batched.cpu.registers[kIP] = kProgramOffset;
  batched.cpu.registers[kSS] = 0;
  batched.cpu.registers[kSP] = 0xFFFE;
  for (int i = 0; i < 8; ++i) {
    batched_ram[kProgramOffset + i] = kOpNop;
  }
  // A NOP is three cycles, so this is comfortably eight of them and no more.
  ASSERT_EQ(PlatformRun(&batched, 8 * 3), kPlatformRunning);

  EXPECT_EQ(batched.cpu.instructions_retired, stepped);
}

}  // namespace

// A software interrupt executing while an acknowledged hardware IRQ is waiting
// must not discard it. Sharing one pending-interrupt slot between the two used
// to lose the IRQ, leaving the PIC with the interrupt permanently in service
// and every lower priority IRQ - notably the keyboard - blocked behind it.
TEST_F(
    PlatformExecutionTest,
    SoftwareInterruptDoesNotStrandAnAcknowledgedInterrupt) {
  enum : uint32_t {
    // Vector table entries, at interrupt number * 4.
    kVectorIRQ0 = 0x08 * 4,
    kVectorInt28 = 0x28 * 4,
    // Handlers, placed clear of the program and the vector table.
    kInt28Handler = 0x0200,
    kIRQ0Handler = 0x0300,
    // Written by the IRQ0 handler to prove it ran.
    kMarker = 0x3000,
  };

  // STI, then a tight loop issuing INT 28h - the same idle interrupt MS-DOS
  // spins on at its command prompt.
  Load({kOpSti, 0xCD, 0x28, 0xEB, 0xFC});
  // INT 28h handler: IRET.
  ram_[kInt28Handler] = 0xCF;
  // IRQ0 handler: MOV byte [kMarker], 0xAA / MOV AL, 20h / OUT 20h, AL / IRET.
  const uint8_t irq0_handler[] = {0xC6, 0x06, kMarker & 0xFF, kMarker >> 8,
                                  0xAA, 0xB0, 0x20,           0xE6,
                                  0x20, 0xCF};
  for (size_t i = 0; i < sizeof(irq0_handler); ++i) {
    ram_[kIRQ0Handler + i] = irq0_handler[i];
  }
  ram_[kVectorInt28 + 2] = kInt28Handler >> 4;
  ram_[kVectorIRQ0 + 2] = kIRQ0Handler >> 4;

  // Initialize the PIC with a vector base of 0x08 and unmask IRQ0.
  WritePortByte(&platform_, 0x20, 0x13);  // ICW1: init, single, ICW4 needed
  WritePortByte(&platform_, 0x21, 0x08);  // ICW2: vector base
  WritePortByte(&platform_, 0x21, 0x01);  // ICW4
  WritePortByte(&platform_, 0x21, 0xFE);  // OCW1: unmask IRQ0

  ASSERT_TRUE(PlatformRaiseIRQ(&platform_, 0));
  RunInstructions(200);

  EXPECT_EQ(ram_[kMarker], 0xAA) << "IRQ0 handler never ran";
  // With the IRQ delivered, the PIC is no longer stuck with it in service.
  EXPECT_EQ(platform_.pic.isr & 0x01, 0);
}
