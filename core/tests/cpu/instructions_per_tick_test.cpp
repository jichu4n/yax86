#include <vector>

#include "cpu.h"
#include "gtest/gtest.h"

namespace {

constexpr uint32_t kMemorySize = 0x10000;
// A power of two, as CPUInit() requires, and large enough that nothing these
// tests run collides.
constexpr uint32_t kNumCacheEntries = 64;

constexpr uint16_t kProgramAddress = 0x0100;
// Where an interrupt handler is put, well clear of the program.
constexpr uint16_t kHandlerAddress = 0x0400;

enum : uint8_t {
  kOpNop = 0x90,
  kOpHlt = 0xF4,
  // INC AX, so that a run's progress is visible in a register as well as in
  // the retired count.
  kOpIncAx = 0x40,
  // IN AL, imm8
  kOpInAlImm8 = 0xE4,
  // OUT imm8, AL
  kOpOutImm8Al = 0xE6,
  // INT 3
  kOpInt3 = 0xCC,
  // IRET
  kOpIret = 0xCF,
  // JMP rel8
  kOpJmpRel8 = 0xEB,
  // POPF
  kOpPopf = 0x9D,
  // PUSH imm16 is not an 8088 instruction, so a flags word is pushed with MOV
  // AX, imm16 / PUSH AX instead.
  kOpMovAxImm16 = 0xB8,
  kOpPushAx = 0x50,
  // Group 3 with /6 is DIV r/m8; the ModR/M below selects DIV BL.
  kOpGroup3Byte = 0xF6,
  kModRmDivBl = 0xF3,
};

// These pin down what ends a run, since every one of them is a point at which
// the emulation would otherwise diverge - an interrupt delivered late, a
// device serviced late, a trap not taken.
//
// Everything here drives CPUTick() directly with a budget set by hand, as
// PlatformRun() does. A run is only ever entered from the decode cache, so
// each test warms it first.
class InstructionsPerTickTest : public ::testing::Test {
 protected:
  void SetUp() override {
    memory_.assign(kMemorySize, 0);
    config_ = CPUConfig{};
    config_.context = this;
    config_.read_memory_byte = ReadMemoryByte;
    config_.write_memory_byte = WriteMemoryByte;
    config_.get_instruction_fetch_window = GetInstructionFetchWindow;
    config_.acknowledge_interrupt = AcknowledgeInterrupt;
    config_.interrupt_request_hint = &interrupt_requested_;
    config_.decode_cache = cache_;
    config_.decode_cache_num_entries = kNumCacheEntries;
    CPUInit(&cpu_, &config_);
    CPUSetDirectDataWindow(&cpu_, memory_.data(), kMemorySize);
    cpu_.registers[kCS] = 0;
    cpu_.registers[kDS] = 0;
    cpu_.registers[kSS] = 0;
    cpu_.registers[kSP] = 0xFFFE;
    cpu_.registers[kIP] = kProgramAddress;
  }

  void Load(uint16_t address, const std::vector<uint8_t>& bytes) {
    for (size_t i = 0; i < bytes.size(); ++i) {
      CPUNotifyMemoryWrite(&cpu_, address + i);
      memory_[address + i] = bytes[i];
    }
  }

  // Puts every instruction of the program into the decode cache, which is
  // where a run takes everything after its first. A budget of zero runs one
  // instruction per tick however warm the cache gets.
  void WarmCache(int num_instructions) {
    for (int i = 0; i < num_instructions; ++i) {
      ASSERT_NE(CPUTick(&cpu_, 0), kCPUTickInvalid);
    }
    Reset();
  }

  // Puts the CPU back at the start of the program with its registers clear,
  // leaving the decode cache as it is.
  void Reset() {
    cpu_.registers[kIP] = kProgramAddress;
    cpu_.registers[kAX] = 0;
    cpu_.registers[kSP] = 0xFFFE;
    cpu_.is_halted = false;
    cpu_.flags = kInitialFlags;
    cpu_.instructions_retired = 0;
  }

  // Runs one tick with a budget too large to be what ends the run, and returns
  // how many instructions it retired.
  uint64_t RunOneTick() {
    const uint64_t before = cpu_.instructions_retired;
    CPUTick(&cpu_, UINT16_MAX);
    return cpu_.instructions_retired - before;
  }

  static uint8_t ReadMemoryByte(CPUState* cpu, uint32_t address) {
    InstructionsPerTickTest* self =
        static_cast<InstructionsPerTickTest*>(cpu->config->context);
    return address < kMemorySize ? self->memory_[address] : 0xFF;
  }

  static void WriteMemoryByte(CPUState* cpu, uint32_t address, uint8_t value) {
    InstructionsPerTickTest* self =
        static_cast<InstructionsPerTickTest*>(cpu->config->context);
    if (address < kMemorySize) {
      self->memory_[address] = value;
    }
  }

  static void GetInstructionFetchWindow(CPUState* cpu, uint32_t address) {
    InstructionsPerTickTest* self =
        static_cast<InstructionsPerTickTest*>(cpu->config->context);
    if (address >= kMemorySize) {
      cpu->instruction_fetch_window.data = nullptr;
      return;
    }
    cpu->instruction_fetch_window.data = self->memory_.data();
    cpu->instruction_fetch_window.start = 0;
    cpu->instruction_fetch_window.end = kMemorySize;
  }

  static bool AcknowledgeInterrupt(CPUState* cpu, uint8_t* vector) {
    InstructionsPerTickTest* self =
        static_cast<InstructionsPerTickTest*>(cpu->config->context);
    if (!self->interrupt_requested_) {
      return false;
    }
    self->interrupt_requested_ = false;
    ++self->interrupts_acknowledged_;
    *vector = 0x08;
    return true;
  }

  std::vector<uint8_t> memory_;
  CPUConfig config_ = {};
  CPUState cpu_ = {};
  CPUDecodeCacheEntry cache_[kNumCacheEntries] = {};
  bool interrupt_requested_ = false;
  int interrupts_acknowledged_ = 0;
};

TEST_F(InstructionsPerTickTest, ARunRetiresSeveralInstructionsInOneTick) {
  Load(kProgramAddress, {kOpIncAx, kOpIncAx, kOpIncAx, kOpHlt});
  WarmCache(3);

  EXPECT_EQ(RunOneTick(), 3u);
  EXPECT_EQ(cpu_.registers[kAX], 3u);
  EXPECT_EQ(cpu_.registers[kIP], kProgramAddress + 3);
}

// The bound is on the tick rather than on the budget: there are twice as many
// cached instructions here as a run may take, and cycles to spare for all of
// them.
TEST_F(InstructionsPerTickTest, ARunIsBoundedToAMaximumNumberOfInstructions) {
  const int available = 2 * kMaxInstructionsPerTick;
  std::vector<uint8_t> program(available, kOpIncAx);
  program.push_back(kOpHlt);
  Load(kProgramAddress, program);
  WarmCache(available);

  EXPECT_EQ(RunOneTick(), (uint64_t)kMaxInstructionsPerTick);
  EXPECT_EQ(cpu_.registers[kAX], kMaxInstructionsPerTick);
}

// A run is entered only from the decode cache, so a CPU that has not run the
// code before runs one instruction per tick however large its budget - which
// is what keeps the 8088 hardware suite a single-instruction test.
TEST_F(InstructionsPerTickTest, AColdCacheRunsOneInstructionPerTick) {
  Load(kProgramAddress, {kOpIncAx, kOpIncAx, kOpIncAx, kOpHlt});

  EXPECT_EQ(RunOneTick(), 1u);
  EXPECT_EQ(cpu_.registers[kAX], 1u);
}

TEST_F(InstructionsPerTickTest, NoDecodeCacheRunsOneInstructionPerTick) {
  Load(kProgramAddress, {kOpIncAx, kOpIncAx, kOpIncAx, kOpHlt});
  WarmCache(3);
  // Clearing the pointer is how a host takes the cache away, which is what the
  // platform does while a memory watchpoint is enabled.
  config_.decode_cache = nullptr;

  EXPECT_EQ(RunOneTick(), 1u);
}

// Zero is what a host that has not asked for runs passes.
TEST_F(InstructionsPerTickTest, NoBudgetRunsOneInstructionPerTick) {
  Load(kProgramAddress, {kOpIncAx, kOpIncAx, kOpIncAx, kOpHlt});
  WarmCache(3);

  const uint64_t before = cpu_.instructions_retired;
  CPUTick(&cpu_, 0);
  EXPECT_EQ(cpu_.instructions_retired - before, 1u);
}

// A run stops where the host would have taken control back anyway, which is
// what keeps a device from being serviced late.
TEST_F(InstructionsPerTickTest, ARunStopsAtTheBudget) {
  Load(kProgramAddress, {kOpIncAx, kOpIncAx, kOpIncAx, kOpIncAx, kOpHlt});
  WarmCache(4);

  // What one of these instructions costs, measured rather than assumed.
  CPUTick(&cpu_, 0);
  const uint16_t one_instruction = cpu_.cycles_this_tick;
  ASSERT_GT(one_instruction, 0);
  Reset();

  // Two instructions' worth of budget is spent by two instructions, so the
  // third does not start.
  CPUTick(&cpu_, (uint16_t)(2 * one_instruction));
  EXPECT_EQ(cpu_.instructions_retired, 2u);
  EXPECT_EQ(cpu_.cycles_this_tick, 2 * one_instruction);
}

// HLT stops the CPU until an interrupt arrives. The test that skips execution
// while halted is made once, before a run starts, so a run has to end here or
// the instructions after a HLT run instead of the wait.
TEST_F(InstructionsPerTickTest, ARunStopsAtHLT) {
  Load(kProgramAddress, {kOpIncAx, kOpHlt, kOpIncAx, kOpIncAx});
  WarmCache(2);

  EXPECT_EQ(RunOneTick(), 2u);
  EXPECT_TRUE(cpu_.is_halted);
  // The INC AX instructions after the HLT did not run.
  EXPECT_EQ(cpu_.registers[kAX], 1u);
}

// An instruction can raise an interrupt on itself, and that interrupt is
// dispatched at the end of the tick. Carrying on would run the instructions
// after it before the handler.
TEST_F(InstructionsPerTickTest, ARunStopsAtASoftwareInterrupt) {
  Load(kProgramAddress, {kOpIncAx, kOpInt3, kOpIncAx, kOpIncAx});
  // INT 3 vectors through IVT entry 3.
  Load(3 * 4, {kHandlerAddress & 0xFF, kHandlerAddress >> 8, 0x00, 0x00});
  Load(kHandlerAddress, {kOpIret});
  WarmCache(2);

  EXPECT_EQ(RunOneTick(), 2u);
  // The tick ended in the handler rather than after the INT.
  EXPECT_EQ(cpu_.registers[kIP], kHandlerAddress);
  EXPECT_EQ(cpu_.registers[kAX], 1u);
}

// A divide error is the same case reached without an explicit INT.
TEST_F(InstructionsPerTickTest, ARunStopsAtADivideError) {
  Load(kProgramAddress, {kOpGroup3Byte, kModRmDivBl, kOpIncAx, kOpIncAx});
  Load(0 * 4, {kHandlerAddress & 0xFF, kHandlerAddress >> 8, 0x00, 0x00});
  Load(kHandlerAddress, {kOpIret});
  WarmCache(1);
  // BL is zero, so DIV BL raises interrupt 0.
  cpu_.registers[kBX] = 0;

  EXPECT_EQ(RunOneTick(), 1u);
  EXPECT_EQ(cpu_.registers[kIP], kHandlerAddress);
}

// A port access is the one instruction whose handler looks at the clock, and
// the host is only told what a tick cost once the tick is over. So a port
// access gets a tick to itself: a run ends before one, which is what leaves it
// reading the clock it would have read unbatched, and ends after one, because
// its own side effects can move the deadline the budget was computed from.
TEST_F(InstructionsPerTickTest, APortReadGetsATickToItself) {
  Load(kProgramAddress, {kOpIncAx, kOpInAlImm8, 0x40, kOpIncAx, kOpHlt});
  WarmCache(4);

  // The run ends before the IN rather than taking it as its second
  // instruction.
  EXPECT_EQ(RunOneTick(), 1u);
  EXPECT_EQ(cpu_.registers[kIP], kProgramAddress + 1);
  // The IN runs on its own, and the run does not carry on past it either.
  EXPECT_EQ(RunOneTick(), 1u);
  EXPECT_EQ(cpu_.registers[kIP], kProgramAddress + 3);
  // Ordinary instructions batch again afterwards.
  EXPECT_EQ(RunOneTick(), 2u);
}

TEST_F(InstructionsPerTickTest, APortWriteGetsATickToItself) {
  Load(kProgramAddress, {kOpIncAx, kOpOutImm8Al, 0x40, kOpIncAx, kOpHlt});
  WarmCache(4);

  EXPECT_EQ(RunOneTick(), 1u);
  EXPECT_EQ(cpu_.registers[kIP], kProgramAddress + 1);
  EXPECT_EQ(RunOneTick(), 1u);
  EXPECT_EQ(cpu_.registers[kIP], kProgramAddress + 3);
  EXPECT_EQ(RunOneTick(), 2u);
}

// TF raises a single-step interrupt after every instruction, taken at the end
// of the tick, so a tick that owes a trap runs exactly one instruction.
TEST_F(InstructionsPerTickTest, TheTrapFlagRunsOneInstructionPerTick) {
  Load(kProgramAddress, {kOpIncAx, kOpIncAx, kOpIncAx, kOpHlt});
  Load(1 * 4, {kHandlerAddress & 0xFF, kHandlerAddress >> 8, 0x00, 0x00});
  Load(kHandlerAddress, {kOpIret});
  WarmCache(3);

  CPUSetFlag(&cpu_, kTF, true);
  EXPECT_EQ(RunOneTick(), 1u);
  EXPECT_EQ(cpu_.registers[kIP], kHandlerAddress);
}

// The other half of the same rule: an instruction that turns TF on must be the
// last of its run, or the instruction after it runs without trapping.
TEST_F(InstructionsPerTickTest, AnInstructionThatSetsTheTrapFlagEndsTheRun) {
  const uint16_t flags_with_tf = kInitialFlags | kTF;
  Load(
      kProgramAddress,
      {kOpMovAxImm16, (uint8_t)(flags_with_tf & 0xFF),
       (uint8_t)(flags_with_tf >> 8), kOpPushAx, kOpPopf, kOpIncAx, kOpHlt});
  Load(1 * 4, {kHandlerAddress & 0xFF, kHandlerAddress >> 8, 0x00, 0x00});
  Load(kHandlerAddress, {kOpIret});
  WarmCache(4);

  // MOV, PUSH and POPF run, and the run ends there. POPF does not trap on
  // itself, so nothing is dispatched yet - what matters is that the INC AX
  // after it did not run inside this tick, because the tick it does run in is
  // the one that owes the trap.
  EXPECT_EQ(RunOneTick(), 3u);
  EXPECT_TRUE(CPUGetFlag(&cpu_, kTF));
  EXPECT_EQ(cpu_.registers[kIP], kProgramAddress + 5);

  // That next tick runs the one instruction and takes the trap.
  EXPECT_EQ(RunOneTick(), 1u);
  EXPECT_EQ(cpu_.registers[kIP], kHandlerAddress);
}

// An external request is only recognized at the end of a tick, so a run ends
// as soon as one could be waiting.
TEST_F(InstructionsPerTickTest, ARunStopsWhenAnInterruptIsRequested) {
  Load(kProgramAddress, {kOpIncAx, kOpIncAx, kOpIncAx, kOpHlt});
  Load(8 * 4, {kHandlerAddress & 0xFF, kHandlerAddress >> 8, 0x00, 0x00});
  Load(kHandlerAddress, {kOpIret});
  WarmCache(3);

  CPUSetFlag(&cpu_, kIF, true);
  interrupt_requested_ = true;

  EXPECT_EQ(RunOneTick(), 1u);
  EXPECT_EQ(interrupts_acknowledged_, 1);
  EXPECT_EQ(cpu_.registers[kIP], kHandlerAddress);
}

// A host that supplies no hint cannot be asked whether a request is waiting
// without an acknowledge call, so it gets one instruction per tick rather than
// a guess.
TEST_F(InstructionsPerTickTest, NoInterruptHintRunsOneInstructionPerTick) {
  Load(kProgramAddress, {kOpIncAx, kOpIncAx, kOpIncAx, kOpHlt});
  WarmCache(3);
  config_.interrupt_request_hint = nullptr;

  EXPECT_EQ(RunOneTick(), 1u);
}

// A run is not limited to straight-line code: the next instruction is looked
// up at CS:IP, wherever the one before it left that.
TEST_F(InstructionsPerTickTest, ARunFollowsATakenBranch) {
  // INC AX / JMP +1 / INC AX (skipped) / INC AX
  Load(
      kProgramAddress,
      {kOpIncAx, kOpJmpRel8, 0x01, kOpIncAx, kOpIncAx, kOpHlt});
  WarmCache(3);

  EXPECT_EQ(RunOneTick(), 3u);
  // The INC AX that the jump went over did not run.
  EXPECT_EQ(cpu_.registers[kAX], 2u);
}

// A run takes its second instruction and every one after it from the cache, so
// it ends where the cache does not have one.
TEST_F(InstructionsPerTickTest, ARunStopsAtAnUncachedInstruction) {
  Load(kProgramAddress, {kOpIncAx, kOpIncAx, kOpIncAx, kOpHlt});
  WarmCache(2);

  // Only the first two instructions were ever run, so only they are cached.
  EXPECT_EQ(RunOneTick(), 2u);
}

// Writing over cached code invalidates it, so a run ends there too - the point
// being that a run can never execute a decode the cache should have thrown
// away.
TEST_F(InstructionsPerTickTest, ARunStopsAtCodeThatHasBeenWrittenOver) {
  Load(kProgramAddress, {kOpIncAx, kOpIncAx, kOpIncAx, kOpHlt});
  WarmCache(3);
  // Rewriting the second instruction, as a host must, discards its decode.
  Load(kProgramAddress + 1, {kOpIncAx});

  EXPECT_EQ(RunOneTick(), 1u);
}

// A stop asked for from inside an instruction - which is what a memory
// watchpoint does - hands control back at that instruction, not three later.
TEST_F(InstructionsPerTickTest, ARunStopsWhenAStopIsRequested) {
  Load(kProgramAddress, {kOpIncAx, kOpIncAx, kOpIncAx, kOpHlt});
  WarmCache(3);

  config_.on_after_execute_instruction = [](CPUState* cpu, const Instruction*) {
    if (cpu->registers[kAX] == 2) {
      CPURequestStop(cpu);
    }
  };

  EXPECT_EQ(CPUTick(&cpu_, UINT16_MAX), kCPUTickStopped);
  EXPECT_EQ(cpu_.instructions_retired, 2u);
}

// The clock is charged what the whole run cost, since that is what the caller
// advances the rest of the machine by.
TEST_F(InstructionsPerTickTest, TheTickIsChargedForTheWholeRun) {
  Load(kProgramAddress, {kOpIncAx, kOpIncAx, kOpIncAx, kOpHlt});
  WarmCache(3);

  CPUTick(&cpu_, 0);
  const uint16_t one_instruction = cpu_.cycles_this_tick;
  Reset();

  EXPECT_EQ(RunOneTick(), 3u);
  EXPECT_EQ(cpu_.cycles_this_tick, 3 * one_instruction);
}

}  // namespace
