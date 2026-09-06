// Tests for CPUTick's per-tick control flow: what it reports, and how the
// halted state interacts with interrupts and the trap flag.

#include <gtest/gtest.h>

#include "./test_helpers.h"
#include "cpu.h"

using namespace std;

class TickTest : public ::testing::Test {};

namespace {

enum : uint8_t {
  kOpNop = 0x90,
  kOpHlt = 0xF4,
  kOpSti = 0xFB,
  kOpCli = 0xFA,
  kOpPopf = 0x9D,
};

// Segment and offset of the single-step handler installed by these tests,
// chosen to land inside the helper's 4KB of memory.
enum : uint16_t {
  kSingleStepHandlerSegment = 0x0020,
  kSingleStepHandlerOffset = 0x0100,
};

// Installs an interrupt vector pointing at segment:offset.
void SetVector(
    CPUTestHelper* helper, uint8_t interrupt_number, uint16_t segment,
    uint16_t offset) {
  const uint16_t vector_offset = interrupt_number * 4;
  helper->memory_[vector_offset + 0] = offset & 0xFF;
  helper->memory_[vector_offset + 1] = offset >> 8;
  helper->memory_[vector_offset + 2] = segment & 0xFF;
  helper->memory_[vector_offset + 3] = segment >> 8;
}

// Whether the CPU is sitting in the single-step handler.
bool InSingleStepHandler(const CPUTestHelper* helper) {
  return helper->cpu_.registers[kCS] == kSingleStepHandlerSegment &&
         helper->cpu_.registers[kIP] == kSingleStepHandlerOffset;
}

// Pushes a flags word for POPF to pop.
void PushFlags(CPUTestHelper* helper, uint16_t flags) {
  helper->cpu_.registers[kSP] -= 2;
  const uint16_t sp = helper->cpu_.registers[kSP];
  helper->memory_[sp] = flags & 0xFF;
  helper->memory_[sp + 1] = flags >> 8;
}

// A stand-in interrupt controller. Like a real one it keeps requesting until
// the CPU runs an acknowledge cycle, and supplies the vector as part of it.
struct FakeController {
  bool requesting = false;
  uint8_t vector = 0;
  int acknowledge_count = 0;
};
FakeController g_controller;

bool AcknowledgeInterrupt(YAX86_UNUSED CPUState* cpu, uint8_t* vector) {
  if (!g_controller.requesting) {
    return false;
  }
  g_controller.requesting = false;
  ++g_controller.acknowledge_count;
  *vector = g_controller.vector;
  return true;
}

// Raises a request, as a controller driving INTR does.
void RaiseINTR(uint8_t vector) {
  g_controller.requesting = true;
  g_controller.vector = vector;
}

// Builds a helper running hand-assembled code, with interrupts dispatched
// through the interrupt vector table rather than the default test callback,
// which throws.
unique_ptr<CPUTestHelper> WithCode(const vector<uint8_t>& code) {
  auto helper = make_unique<CPUTestHelper>();
  helper->LoadCOM(code);
  helper->cpu_.config->handle_interrupt = nullptr;
  g_controller = FakeController();
  helper->cpu_.config->acknowledge_interrupt = AcknowledgeInterrupt;
  helper->cpu_.registers[kSS] = 0;
  helper->cpu_.registers[kSP] = 0x0800;
  return helper;
}

}  // namespace

// ============================================================================
// What a tick reports
// ============================================================================

TEST_F(TickTest, HaltingInstructionReportsExecuted) {
  auto helper = WithCode({kOpHlt, kOpNop});

  // The tick that runs HLT executed an instruction, even though the CPU ends
  // it halted. Reporting kCPUTickHalted here would hide the instruction from
  // anything counting or single-stepping them.
  EXPECT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);
  EXPECT_TRUE(helper->cpu_.is_halted);

  // Subsequent ticks run nothing.
  EXPECT_EQ(CPUTick(&helper->cpu_), kCPUTickHalted);
  EXPECT_EQ(CPUTick(&helper->cpu_), kCPUTickHalted);
  EXPECT_TRUE(helper->cpu_.is_halted);
}

TEST_F(TickTest, HaltedCPUDoesNotAdvanceIP) {
  auto helper = WithCode({kOpHlt, kOpNop});
  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);
  const uint16_t ip_after_halt = helper->cpu_.registers[kIP];

  for (int i = 0; i < 4; ++i) {
    ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickHalted);
  }

  EXPECT_EQ(helper->cpu_.registers[kIP], ip_after_halt);
}

// ============================================================================
// Interrupts and the halted state
// ============================================================================

TEST_F(TickTest, PendingInterruptWakesHaltedCPU) {
  auto helper = WithCode({kOpHlt, kOpNop});
  // Point the vector for interrupt 0x20 at 0010:0100, which is linear 0x200 -
  // inside the test's memory, so the handler can actually be executed. A
  // non-zero segment proves CS was loaded from the vector.
  const uint16_t kVectorOffset = 0x20 * 4;
  helper->memory_[kVectorOffset + 0] = 0x00;
  helper->memory_[kVectorOffset + 1] = 0x01;
  helper->memory_[kVectorOffset + 2] = 0x10;
  helper->memory_[kVectorOffset + 3] = 0x00;
  helper->memory_[0x200] = kOpNop;

  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);
  ASSERT_TRUE(helper->cpu_.is_halted);
  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickHalted);

  // A hardware interrupt arrives, as the platform would inject it.
  CPURaiseInternalInterrupt(&helper->cpu_, 0x20);
  EXPECT_EQ(CPUTick(&helper->cpu_), kCPUTickHalted);

  // The interrupt cleared the halted state and vectored to the handler, even
  // though no instruction ran on that tick.
  EXPECT_FALSE(helper->cpu_.is_halted);
  EXPECT_EQ(helper->cpu_.registers[kCS], 0x0010);
  EXPECT_EQ(helper->cpu_.registers[kIP], 0x0100);

  // Execution resumes normally afterwards.
  EXPECT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);
}

// ============================================================================
// The trap flag
// ============================================================================

TEST_F(TickTest, TrapFlagRaisesSingleStepAfterAnInstruction) {
  auto helper = WithCode({kOpNop, kOpNop});
  // Point the single-step vector at 1111:2222.
  const uint16_t kVectorOffset = kInterruptSingleStep * 4;
  helper->memory_[kVectorOffset + 0] = 0x22;
  helper->memory_[kVectorOffset + 1] = 0x22;
  helper->memory_[kVectorOffset + 2] = 0x11;
  helper->memory_[kVectorOffset + 3] = 0x11;
  CPUSetFlag(&helper->cpu_, kTF, true);

  EXPECT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);

  EXPECT_EQ(helper->cpu_.registers[kCS], 0x1111);
  EXPECT_EQ(helper->cpu_.registers[kIP], 0x2222);
  // Entering the handler clears TF.
  EXPECT_FALSE(CPUGetFlag(&helper->cpu_, kTF));
}

TEST_F(TickTest, HaltedCPUDoesNotTrapOnTrapFlag) {
  auto helper = WithCode({kOpHlt, kOpNop});
  const uint16_t kVectorOffset = kInterruptSingleStep * 4;
  helper->memory_[kVectorOffset + 0] = 0x22;
  helper->memory_[kVectorOffset + 1] = 0x22;
  helper->memory_[kVectorOffset + 2] = 0x11;
  helper->memory_[kVectorOffset + 3] = 0x11;

  // Halt first, then set TF, so that the trap flag is set while the CPU is
  // already halted.
  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);
  ASSERT_TRUE(helper->cpu_.is_halted);
  CPUSetFlag(&helper->cpu_, kTF, true);

  // The trap flag traps after an instruction executes. A halted CPU executes
  // none, so it must stay halted rather than waking itself up and then
  // trapping again on every tick.
  for (int i = 0; i < 8; ++i) {
    ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickHalted) << "tick " << i;
    ASSERT_TRUE(helper->cpu_.is_halted) << "tick " << i;
    ASSERT_NE(helper->cpu_.registers[kCS], 0x1111) << "tick " << i;
  }
  EXPECT_TRUE(CPUGetFlag(&helper->cpu_, kTF));
}

TEST_F(TickTest, HaltWithTrapFlagTrapsOnceForTheHaltItself) {
  auto helper = WithCode({kOpSti, kOpHlt});
  const uint16_t kVectorOffset = kInterruptSingleStep * 4;
  helper->memory_[kVectorOffset + 0] = 0x22;
  helper->memory_[kVectorOffset + 1] = 0x22;
  helper->memory_[kVectorOffset + 2] = 0x11;
  helper->memory_[kVectorOffset + 3] = 0x11;

  // STI executes and traps, clearing TF. Re-arm it, then run HLT.
  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);
  helper->cpu_.registers[kCS] = 0;
  helper->cpu_.registers[kIP] = kCOMFileLoadOffset + 1;
  CPUSetFlag(&helper->cpu_, kTF, true);

  // HLT is an instruction, so the trap does fire for it - and dispatching the
  // interrupt clears the halted state, which is what real hardware does.
  EXPECT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);
  EXPECT_EQ(helper->cpu_.registers[kCS], 0x1111);
  EXPECT_FALSE(helper->cpu_.is_halted);
}

// ============================================================================
// When the trap flag is sampled
// ============================================================================
//
// The trap flag is read before the instruction runs, so the decision to trap
// reflects TF as it was at the instruction boundary. MartyPC models the same
// rule as a pair of enable/disable delays, validated against real hardware.

TEST_F(TickTest, PopfSettingTrapFlagDoesNotTrapOnItself) {
  auto helper = WithCode({kOpPopf, kOpNop, kOpNop});
  SetVector(
      helper.get(), kInterruptSingleStep, kSingleStepHandlerSegment,
      kSingleStepHandlerOffset);
  helper->memory_[(kSingleStepHandlerSegment << 4) + kSingleStepHandlerOffset] =
      kOpNop;

  // POPF loads a flags word with TF set, starting from TF clear.
  CPUSetFlag(&helper->cpu_, kTF, false);
  PushFlags(helper.get(), kInitialFlags | kTF);

  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);

  // TF is now set, but the instruction that set it must not trap on itself.
  EXPECT_TRUE(CPUGetFlag(&helper->cpu_, kTF));
  EXPECT_FALSE(InSingleStepHandler(helper.get()));

  // The following instruction does trap.
  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);
  EXPECT_TRUE(InSingleStepHandler(helper.get()));
}

TEST_F(TickTest, PopfClearingTrapFlagStillTrapsOnce) {
  auto helper = WithCode({kOpPopf, kOpNop});
  SetVector(
      helper.get(), kInterruptSingleStep, kSingleStepHandlerSegment,
      kSingleStepHandlerOffset);

  // POPF loads a flags word with TF clear, starting from TF set.
  CPUSetFlag(&helper->cpu_, kTF, true);
  PushFlags(helper.get(), kInitialFlags);

  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);

  // The trap still fires for the instruction TF was set during, even though
  // that instruction cleared it.
  EXPECT_TRUE(InSingleStepHandler(helper.get()));
}

TEST_F(TickTest, DispatchedInterruptTakesPrecedenceOverSingleStep) {
  auto helper = WithCode({kOpNop, kOpNop});
  SetVector(
      helper.get(), kInterruptSingleStep, kSingleStepHandlerSegment,
      kSingleStepHandlerOffset);
  // A hardware interrupt vectoring somewhere else entirely.
  SetVector(helper.get(), 0x20, 0x0010, 0x0100);

  CPUSetFlag(&helper->cpu_, kTF, true);
  CPURaiseInternalInterrupt(&helper->cpu_, 0x20);

  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);

  // Single-stepping is the lowest priority interrupt source at an instruction
  // boundary, so the hardware interrupt takes its place rather than both
  // firing in one tick.
  EXPECT_EQ(helper->cpu_.registers[kCS], 0x0010);
  EXPECT_EQ(helper->cpu_.registers[kIP], 0x0100);
  EXPECT_FALSE(InSingleStepHandler(helper.get()));
}

// ============================================================================
// External interrupt requests
// ============================================================================
//
// An interrupt controller asserts INTR independently of anything the CPU is
// doing, so an external request and an internal one can be outstanding at the
// same moment. They are tracked separately: sharing one slot let an INT
// instruction silently discard an acknowledged IRQ, leaving the controller
// waiting forever for an end-of-interrupt.

TEST_F(TickTest, SoftwareInterruptDoesNotDeassertINTR) {
  // INT 28h, then instructions to return to.
  auto helper = WithCode({0xCD, 0x28, kOpNop, kOpNop});
  // INT 28h handler at 0050:0100 (linear 0x600), just an IRET.
  SetVector(helper.get(), 0x28, 0x0050, 0x0100);
  helper->memory_[0x600] = 0xCF;
  // IRQ0 handler at 0060:0100 (linear 0x700).
  SetVector(helper.get(), 0x08, 0x0060, 0x0100);
  helper->memory_[0x700] = kOpNop;
  CPUSetFlag(&helper->cpu_, kTF, false);
  CPUSetFlag(&helper->cpu_, kIF, true);

  // The controller asserts IRQ0 just before the INT instruction executes.
  RaiseINTR(0x08);

  // The INT instruction is taken first, since it was raised by the instruction
  // that just executed.
  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);
  EXPECT_EQ(helper->cpu_.registers[kCS], 0x0050);
  EXPECT_EQ(helper->cpu_.registers[kIP], 0x0100);
  // The external request must survive it: no acknowledge cycle has run, so the
  // controller is still requesting and no vector has been produced.
  EXPECT_TRUE(g_controller.requesting);
  EXPECT_EQ(g_controller.acknowledge_count, 0);

  // IRET restores IF, and the request is then taken.
  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);
  EXPECT_EQ(helper->cpu_.registers[kCS], 0x0060);
  EXPECT_EQ(helper->cpu_.registers[kIP], 0x0100);
  EXPECT_FALSE(g_controller.requesting);
  // Acknowledged exactly once, at the moment it was taken.
  EXPECT_EQ(g_controller.acknowledge_count, 1);
}

TEST_F(TickTest, AssertedINTRWaitsForInterruptsToBeEnabled) {
  auto helper = WithCode({kOpCli, kOpNop, kOpSti, kOpNop, kOpNop});
  SetVector(helper.get(), 0x08, 0x0060, 0x0100);
  helper->memory_[0x700] = kOpNop;

  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);  // CLI
  RaiseINTR(0x08);

  // Not taken while interrupts are disabled - and not thrown away either.
  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);  // NOP
  EXPECT_TRUE(g_controller.requesting);
  EXPECT_NE(helper->cpu_.registers[kCS], 0x0060);

  // Once interrupts are enabled again the request is taken.
  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);  // STI
  for (int i = 0; i < 2 && g_controller.requesting; ++i) {
    ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);
  }
  EXPECT_FALSE(g_controller.requesting);
  EXPECT_EQ(helper->cpu_.registers[kCS], 0x0060);
  EXPECT_EQ(helper->cpu_.registers[kIP], 0x0100);
}

// ============================================================================
// The interrupt request hint
// ============================================================================

// The hint exists so that the CPU can skip the acknowledge cycle it would
// otherwise run at every instruction boundary with interrupts enabled. What
// makes that safe is that it is only ever read as permission not to ask.
TEST_F(TickTest, HintReadingTrueTakesTheInterrupt) {
  auto helper = WithCode({kOpSti, kOpNop, kOpNop, kOpNop});
  SetVector(helper.get(), 0x08, 0x0060, 0x0100);
  helper->memory_[0x700] = kOpNop;

  bool hint = true;
  helper->cpu_.config->interrupt_request_hint = &hint;

  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);  // STI
  RaiseINTR(0x08);
  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);  // NOP

  EXPECT_EQ(g_controller.acknowledge_count, 1);
  EXPECT_EQ(helper->cpu_.registers[kCS], 0x0060);
}

// A hint reading false is taken at its word: the acknowledge cycle does not
// run at all, so a controller that lets it read false while it is requesting
// stalls its own interrupt indefinitely. That is the whole cost of the
// contract, and it is why a host that cannot guarantee the flag rises with
// every request supplies none.
TEST_F(TickTest, HintReadingFalseSuppressesTheAcknowledgeCycle) {
  auto helper = WithCode({kOpSti, kOpNop, kOpNop, kOpNop});
  SetVector(helper.get(), 0x08, 0x0060, 0x0100);
  helper->memory_[0x700] = kOpNop;

  bool hint = false;
  helper->cpu_.config->interrupt_request_hint = &hint;

  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);  // STI
  RaiseINTR(0x08);
  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);  // NOP
  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);  // NOP

  EXPECT_EQ(g_controller.acknowledge_count, 0);
  EXPECT_TRUE(g_controller.requesting);
  EXPECT_NE(helper->cpu_.registers[kCS], 0x0060);

  // Nothing is lost by the wait - the request is still there to be taken once
  // the hint reports it, exactly as a real controller keeps driving INTR.
  hint = true;
  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);
  EXPECT_EQ(g_controller.acknowledge_count, 1);
  EXPECT_EQ(helper->cpu_.registers[kCS], 0x0060);
}

// A host that supplies no hint is asked at every boundary, as one always was.
// The mock configs throughout these tests leave it NULL, so this states what
// the rest of the file depends on.
TEST_F(TickTest, NoHintAsksTheControllerEveryTime) {
  auto helper = WithCode({kOpSti, kOpNop, kOpNop});
  SetVector(helper.get(), 0x08, 0x0060, 0x0100);
  helper->memory_[0x700] = kOpNop;

  ASSERT_EQ(helper->cpu_.config->interrupt_request_hint, nullptr);

  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);  // STI
  RaiseINTR(0x08);
  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);  // NOP

  EXPECT_EQ(g_controller.acknowledge_count, 1);
  EXPECT_EQ(helper->cpu_.registers[kCS], 0x0060);
}

TEST_F(TickTest, AssertedINTRWakesHaltedCPU) {
  auto helper = WithCode({kOpSti, kOpHlt});
  SetVector(helper.get(), 0x08, 0x0060, 0x0100);
  helper->memory_[0x700] = kOpNop;

  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);  // STI
  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);  // HLT
  ASSERT_TRUE(helper->cpu_.is_halted);

  RaiseINTR(0x08);
  EXPECT_EQ(CPUTick(&helper->cpu_), kCPUTickHalted);

  EXPECT_FALSE(helper->cpu_.is_halted);
  EXPECT_EQ(helper->cpu_.registers[kCS], 0x0060);
}
