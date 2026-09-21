#include <memory>
#include <vector>

#include "gtest/gtest.h"
#include "platform.h"

namespace {

// Address at which test programs are loaded. Well clear of the interrupt
// vector table at 0x0000-0x03FF.
constexpr uint16_t kProgramOffset = 0x0100;
// Address used by tests that touch data. Well clear of the program.
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
    platform_.config.physical_memory_size = sizeof(ram_);
    platform_.config.context = this;
    platform_.config.physical_memory = ram_;
    platform_.config.vram = vram_;

    ASSERT_TRUE(PlatformInit(&platform_));

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

  PlatformState platform_ = {};
  uint8_t ram_[64 * 1024] = {0};
  uint8_t vram_[kCGAVRAMSize] = {0};
};

TEST_F(PlatformExecutionTest, TickReportsRunning) {
  Load({kOpNop, kOpNop});

  EXPECT_EQ(PlatformTick(&platform_), kPlatformRunning);
  EXPECT_EQ(ip(), kProgramOffset + 1);
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

// The platform counts retired instructions so that a caller does not have to
// drive it one instruction at a time to find out - which is what a benchmark
// harness would otherwise do, giving up PlatformRun()'s batching for a number
// the platform already has.
// How many NOPs the two batching tests below run. Any number a run may take in
// one go will do.
constexpr uint16_t kNumProgramInstructions = 8;

// PlatformRun() batches instructions into a tick, which PlatformTick() does
// not. These pin down both halves of that, and the two debug features that
// take it away again.

// A run is entered only from the decode cache, so the program is run through
// once to fill it before any of this is visible.
TEST_F(PlatformExecutionTest, PlatformRunBatchesCachedInstructions) {
  Load(std::vector<uint8_t>(kNumProgramInstructions, kOpNop));

  // What one instruction costs, from a tick that runs exactly one.
  ASSERT_EQ(PlatformTick(&platform_), kPlatformRunning);
  const uint16_t one_instruction = platform_.cpu.cycles_this_tick;
  ASSERT_GT(one_instruction, 0);
  ASSERT_EQ(RunInstructions(kNumProgramInstructions - 1), kPlatformRunning);

  // All cached and none needing the host's attention, so one tick runs the
  // lot - which holds only while the program is shorter than a run may be.
  ASSERT_LE(kNumProgramInstructions, kMaxInstructionsPerTick);
  platform_.cpu.registers[kIP] = kProgramOffset;
  ASSERT_EQ(PlatformRun(&platform_, one_instruction), kPlatformRunning);
  // One tick did all of them, so the clock was charged for all at once.
  EXPECT_EQ(
      platform_.cpu.cycles_this_tick,
      kNumProgramInstructions * one_instruction);
  EXPECT_EQ(ip(), kProgramOffset + kNumProgramInstructions);
}

// PlatformTick() promises one instruction, which is what makes it the entry
// point to step a machine with. A warm cache must not change that.
TEST_F(PlatformExecutionTest, PlatformTickRunsOneInstructionWithAWarmCache) {
  Load({kOpNop, kOpNop, kOpNop, kOpNop});
  ASSERT_EQ(RunInstructions(4), kPlatformRunning);

  platform_.cpu.registers[kIP] = kProgramOffset;
  const uint64_t before = CPUInstructionsRetired(&platform_.cpu);
  ASSERT_EQ(PlatformTick(&platform_), kPlatformRunning);
  EXPECT_EQ(CPUInstructionsRetired(&platform_.cpu) - before, 1u);
  EXPECT_EQ(ip(), kProgramOffset + 1);
}

// A whole machine, so that two of them can be run side by side.
struct Machine {
  PlatformState platform = {};
  uint8_t ram[64 * 1024] = {0};
  uint8_t vram[kCGAVRAMSize] = {0};
};

// What a run of the program below came to. Exact, so that any difference at
// all between the two ways of driving it shows up.
struct Outcome {
  uint32_t ticks;
  uint64_t retired;
  uint16_t ax;
  uint16_t bx;
  uint16_t cx;
  bool halted;
};

// Runs a program that loops until the timer interrupt has fired three times,
// with the machine driven either one instruction at a time or in runs.
//
// The program reads a port every time round its loop and takes an interrupt
// that acknowledges one, so it covers both of the things a run has to stop
// for, and its exit condition is a count the interrupt handler keeps - which
// makes where it ends a property of the emulation rather than of the driver.
Outcome RunTimerProgram(bool batched) {
  auto machine = std::unique_ptr<Machine>(new Machine());
  PlatformState* platform = &machine->platform;
  platform->config.physical_memory_size = sizeof(machine->ram);
  platform->config.physical_memory = machine->ram;
  platform->config.vram = machine->vram;
  EXPECT_TRUE(PlatformInit(platform));

  // STI / loop: INC AX / IN AL, 0x40 / ADD BL, AL / IN AL, 0x40 /
  // ADD BH, AL / CMP CX, 3 / JB loop / HLT
  //
  // Two things matter about this shape. The port reads are accumulated rather
  // than discarded, so BX records every value the timer handed back - what the
  // PIT reports depends on how far the clock has been advanced when the read
  // is made, so a stale one shows up here. And the first of them sits one
  // instruction into what would otherwise be a run, which is where a read
  // would see a clock that stops short of the instruction before it.
  const std::vector<uint8_t> program = {kOpSti, 0x40, 0xE4, 0x40,  0x00, 0xC3,
                                        0xE4,   0x40, 0x00, 0xC7,  0x83, 0xF9,
                                        0x03,   0x72, 0xF2, kOpHlt};
  for (size_t i = 0; i < program.size(); ++i) {
    machine->ram[kProgramOffset + i] = program[i];
  }
  // IRQ0 handler: INC CX / MOV AL, 0x20 / OUT 0x20, AL / IRET
  const uint16_t kHandler = 0x0400;
  const std::vector<uint8_t> handler = {0x41, 0xB0, 0x20, 0xE6, 0x20, 0xCF};
  for (size_t i = 0; i < handler.size(); ++i) {
    machine->ram[kHandler + i] = handler[i];
  }
  // Interrupt vector 8, which is where the PIC is about to be told to put
  // IRQ0.
  machine->ram[8 * 4] = kHandler & 0xFF;
  machine->ram[8 * 4 + 1] = kHandler >> 8;

  // Vector base 0x08, IRQ0 alone unmasked.
  WritePortByte(platform, 0x20, 0x13);
  WritePortByte(platform, 0x21, 0x08);
  WritePortByte(platform, 0x21, 0x01);
  WritePortByte(platform, 0x21, 0xFE);
  // Channel 0, both bytes, square wave, reload 0x1000 - short enough that
  // three interrupts arrive quickly.
  WritePortByte(platform, 0x43, 0x36);
  WritePortByte(platform, 0x40, 0x00);
  WritePortByte(platform, 0x40, 0x10);

  platform->cpu.registers[kCS] = 0;
  platform->cpu.registers[kIP] = kProgramOffset;
  platform->cpu.registers[kSS] = 0;
  platform->cpu.registers[kSP] = 0xFFFE;

  // A budget of one cycle runs exactly one tick, so the batched machine takes
  // the same number of steps as the stepped one and neither runs on past the
  // HLT. What differs is only how many instructions a tick is allowed.
  for (int i = 0; i < 2000000 && !platform->cpu.is_halted; ++i) {
    if (batched) {
      PlatformRun(platform, 1);
    } else {
      PlatformTick(platform);
    }
  }

  Outcome outcome = {};
  outcome.ticks = platform->ticks;
  outcome.retired = CPUInstructionsRetired(&platform->cpu);
  outcome.ax = platform->cpu.registers[kAX];
  outcome.bx = platform->cpu.registers[kBX];
  outcome.cx = platform->cpu.registers[kCX];
  outcome.halted = platform->cpu.is_halted;
  return outcome;
}

// The claim a run rests on: it changes how often the host hears from the
// machine, and nothing else. Every device still sees every cycle, every
// interrupt is delivered at the instruction boundary it would have been, and
// the guest cannot tell.
//
// This is a stronger check than the dos-boot invariant on hardware, which is
// measured through a harness that polls the screen on emulated-cycle
// boundaries and so moves when a batch ends at a different cycle. Here there
// is no host in the loop and the comparison is exact.
TEST(PlatformBatchingTest, ABatchedRunIsIndistinguishableFromSteppingIt) {
  const Outcome stepped = RunTimerProgram(false);
  const Outcome batched = RunTimerProgram(true);

  ASSERT_TRUE(stepped.halted);
  ASSERT_TRUE(batched.halted);
  // The program only halts once the handler has run three times, so this is
  // also what says the interrupts were delivered at all.
  ASSERT_EQ(stepped.cx, 3u);

  EXPECT_EQ(batched.ticks, stepped.ticks);
  EXPECT_EQ(batched.retired, stepped.retired);
  EXPECT_EQ(batched.ax, stepped.ax);
  EXPECT_EQ(batched.bx, stepped.bx);
  EXPECT_EQ(batched.cx, stepped.cx);
}

TEST_F(PlatformExecutionTest, CountsRetiredInstructions) {
  Load({kOpNop, kOpNop, kOpNop, kOpNop});

  EXPECT_EQ(CPUInstructionsRetired(&platform_.cpu), 0u);
  ASSERT_EQ(RunInstructions(4), kPlatformRunning);
  EXPECT_EQ(CPUInstructionsRetired(&platform_.cpu), 4u);
}

// A halted CPU retires nothing, however long the machine is left running. The
// clock still advances, which is what lets an interrupt arrive and wake it.
TEST_F(PlatformExecutionTest, HaltedTicksRetireNoInstructions) {
  Load({kOpSti, kOpHlt});
  ASSERT_EQ(RunInstructions(2), kPlatformRunning);
  const uint64_t retired_at_halt = CPUInstructionsRetired(&platform_.cpu);
  const uint32_t ticks_at_halt = platform_.ticks;

  ASSERT_EQ(RunInstructions(100), kPlatformRunning);

  EXPECT_EQ(CPUInstructionsRetired(&platform_.cpu), retired_at_halt);
  EXPECT_GT(platform_.ticks, ticks_at_halt);
}

// PlatformRun() must agree with PlatformTick() about what an instruction is,
// or the count would depend on how the caller chose to drive the machine.
TEST_F(PlatformExecutionTest, BatchedRunCountsTheSameInstructions) {
  Load({kOpNop, kOpNop, kOpNop, kOpNop, kOpNop, kOpNop, kOpNop, kOpNop});
  ASSERT_EQ(RunInstructions(8), kPlatformRunning);
  const uint64_t stepped = CPUInstructionsRetired(&platform_.cpu);

  PlatformState batched = {};
  static uint8_t batched_ram[64 * 1024] = {0};
  static uint8_t batched_vram[kCGAVRAMSize] = {0};
  batched.config.physical_memory_size = sizeof(batched_ram);
  batched.config.physical_memory = batched_ram;
  batched.config.vram = batched_vram;
  ASSERT_TRUE(PlatformInit(&batched));
  batched.cpu.registers[kCS] = 0;
  batched.cpu.registers[kIP] = kProgramOffset;
  batched.cpu.registers[kSS] = 0;
  batched.cpu.registers[kSP] = 0xFFFE;
  for (int i = 0; i < 8; ++i) {
    batched_ram[kProgramOffset + i] = kOpNop;
  }
  // A NOP is three cycles, so this is comfortably eight of them and no more.
  ASSERT_EQ(PlatformRun(&batched, 8 * 3), kPlatformRunning);

  EXPECT_EQ(CPUInstructionsRetired(&batched.cpu), stepped);
}

}  // namespace

// The CPU skips its acknowledge cycle while the hint reads false, so the
// platform has to point it at the PIC and the PIC has to keep it current. The
// IRQ0 test below fails outright if this is wired to something stuck false;
// this names the wiring so that dropping it is not a diffuse failure.
TEST_F(PlatformExecutionTest, TheCPUsInterruptHintTracksThePIC) {
  ASSERT_EQ(
      platform_.cpu.config.interrupt_request_hint,
      &platform_.pic.has_unmasked_request);

  // Vector base 0x08, everything masked.
  WritePortByte(&platform_, 0x20, 0x13);
  WritePortByte(&platform_, 0x21, 0x08);
  WritePortByte(&platform_, 0x21, 0x01);
  WritePortByte(&platform_, 0x21, 0xFF);
  EXPECT_FALSE(*platform_.cpu.config.interrupt_request_hint);

  // A masked request is not one the CPU could take, so the hint stays false.
  ASSERT_TRUE(PlatformRaiseIRQ(&platform_, 0));
  EXPECT_FALSE(*platform_.cpu.config.interrupt_request_hint);

  // Unmasking it makes the request takeable, with no IRQ raised in between.
  WritePortByte(&platform_, 0x21, 0xFE);
  EXPECT_TRUE(*platform_.cpu.config.interrupt_request_hint);
}

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
