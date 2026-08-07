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
    config_.physical_memory_size = 64 * 1024;
    config_.context = this;
    config_.read_physical_memory_byte = [](PlatformState* p,
                                           uint32_t addr) -> uint8_t {
      auto* test = static_cast<PlatformExecutionTest*>(p->config->context);
      return addr < sizeof(test->ram_) ? test->ram_[addr] : 0xFF;
    };
    config_.write_physical_memory_byte = [](PlatformState* p, uint32_t addr,
                                            uint8_t val) {
      auto* test = static_cast<PlatformExecutionTest*>(p->config->context);
      if (addr < sizeof(test->ram_)) {
        test->ram_[addr] = val;
      }
    };

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

  PlatformConfig config_ = {0};
  PlatformState platform_ = {};
  uint8_t ram_[64 * 1024] = {0};
};

TEST_F(PlatformExecutionTest, TickReportsRunning) {
  Load({kOpNop, kOpNop});

  EXPECT_EQ(PlatformTick(&platform_), kPlatformRunning);
  EXPECT_EQ(ip(), kProgramOffset + 1);
  EXPECT_EQ(PlatformGetStopInfo(&platform_), nullptr);
}

TEST_F(PlatformExecutionTest, RunConsumesFullBudget) {
  Load({kOpNop, kOpNop, kOpNop, kOpNop});

  EXPECT_EQ(PlatformRun(&platform_, 4), kPlatformRunning);
  EXPECT_EQ(ip(), kProgramOffset + 4);
}

TEST_F(PlatformExecutionTest, UnimplementedEncodingIsReportedAsInvalid) {
  Load({kOpNop, kOpGroup4, kModRMGroup4Reg2, kOpNop});

  EXPECT_EQ(PlatformRun(&platform_, 8), kPlatformInvalid);
  // IP has advanced past the offending instruction, so the caller can choose
  // to keep going.
  EXPECT_EQ(ip(), kProgramOffset + 3);
}

TEST_F(PlatformExecutionTest, RunResumesAfterInvalidInstruction) {
  Load({kOpNop, kOpGroup4, kModRMGroup4Reg2, kOpNop, kOpNop});

  ASSERT_EQ(PlatformRun(&platform_, 4), kPlatformInvalid);
  // The offending tick is counted, so a host that treats an invalid
  // instruction as non-fatal can resume and make progress.
  const uint32_t ticks_before = platform_.ticks;
  EXPECT_EQ(PlatformRun(&platform_, 2), kPlatformRunning);
  EXPECT_GT(platform_.ticks, ticks_before);
  EXPECT_EQ(ip(), kProgramOffset + 5);
}

TEST_F(PlatformExecutionTest, HaltWithInterruptsDisabledIsReportedAsHung) {
  Load({kOpCli, kOpHlt});

  EXPECT_EQ(PlatformRun(&platform_, 8), kPlatformHung);
  EXPECT_TRUE(platform_.cpu.is_halted);
}

TEST_F(PlatformExecutionTest, HaltWithInterruptsEnabledKeepsRunning) {
  Load({kOpSti, kOpHlt});

  // A halted CPU that can still be woken is not a stop: the rest of the
  // machine has to keep ticking so that an interrupt can arrive.
  EXPECT_EQ(PlatformRun(&platform_, 64), kPlatformRunning);
  EXPECT_TRUE(platform_.cpu.is_halted);
  // The PIT keeps advancing even though the CPU is halted.
  EXPECT_EQ(platform_.ticks, 64u);
}

TEST_F(PlatformExecutionTest, BreakpointStopsBeforeExecutingInstruction) {
  Load({kOpNop, kOpMovAlImm8, 0x42, kOpNop});

  const int8_t index = PlatformAddBreakpoint(&platform_, 0, kProgramOffset + 1);
  ASSERT_GE(index, 0);

  EXPECT_EQ(PlatformRun(&platform_, 8), kPlatformStopped);
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
  ASSERT_EQ(PlatformRun(&platform_, 8), kPlatformStopped);

  // Resuming must execute the instruction under the breakpoint rather than
  // stopping on it again. Two ticks run the MOV and the trailing NOP.
  EXPECT_EQ(PlatformRun(&platform_, 2), kPlatformRunning);
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

  EXPECT_EQ(PlatformRun(&platform_, 4), kPlatformRunning);
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

  EXPECT_EQ(PlatformRun(&platform_, 8), kPlatformStopped);
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

  EXPECT_EQ(PlatformRun(&platform_, 3), kPlatformRunning);
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

  EXPECT_EQ(PlatformRun(&platform_, 3), kPlatformStopped);

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

  EXPECT_EQ(PlatformRun(&platform_, 3), kPlatformRunning);
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
    EXPECT_EQ(PlatformRun(&platform_, 100), kPlatformStopped) << "step " << i;
    EXPECT_EQ(ip(), kProgramOffset + i);
    const PlatformStopInfo* stop_info = PlatformGetStopInfo(&platform_);
    ASSERT_NE(stop_info, nullptr);
    EXPECT_EQ(stop_info->reason, kPlatformStopStep);
    EXPECT_EQ(stop_info->ip, kProgramOffset + i);
  }

  PlatformSetStepMode(&platform_, false);
  EXPECT_EQ(PlatformRun(&platform_, 4), kPlatformRunning);
}

TEST_F(PlatformExecutionTest, StepModeStopsOnHaltingInstruction) {
  // STI first, so that the halt is wakeable and does not report as hung.
  Load({kOpSti, kOpHlt, kOpNop});
  PlatformSetStepMode(&platform_, true);

  EXPECT_EQ(PlatformRun(&platform_, 100), kPlatformStopped);
  EXPECT_EQ(ip(), kProgramOffset + 1);

  // HLT is an instruction, so stepping must stop after it runs rather than
  // running away because the CPU happens to be halted afterwards.
  EXPECT_EQ(PlatformRun(&platform_, 100), kPlatformStopped);
  EXPECT_EQ(ip(), kProgramOffset + 2);
  const PlatformStopInfo* stop_info = PlatformGetStopInfo(&platform_);
  ASSERT_NE(stop_info, nullptr);
  EXPECT_EQ(stop_info->reason, kPlatformStopStep);
  EXPECT_TRUE(platform_.cpu.is_halted);

  // Once halted, no instruction retires, so stepping does not stop again -
  // the machine simply keeps ticking until an interrupt wakes the CPU.
  EXPECT_EQ(PlatformRun(&platform_, 100), kPlatformRunning);
  EXPECT_TRUE(platform_.cpu.is_halted);
}

TEST_F(PlatformExecutionTest, HungIsReportedAheadOfAStepStop) {
  Load({kOpCli, kOpHlt});
  PlatformSetStepMode(&platform_, true);

  EXPECT_EQ(PlatformRun(&platform_, 100), kPlatformStopped);

  // The HLT retires, which would otherwise be a step stop, but a CPU that can
  // never be woken is the more useful thing to report.
  EXPECT_EQ(PlatformRun(&platform_, 100), kPlatformHung);
}

}  // namespace
