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

// Builds a helper running hand-assembled code, with interrupts dispatched
// through the interrupt vector table rather than the default test callback,
// which throws.
unique_ptr<CPUTestHelper> WithCode(const vector<uint8_t>& code) {
  auto helper = make_unique<CPUTestHelper>();
  helper->LoadCOM(code);
  helper->cpu_.config->handle_interrupt = nullptr;
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
  CPUSetPendingInterrupt(&helper->cpu_, 0x20);
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
  CPUSetPendingInterrupt(&helper->cpu_, 0x20);

  ASSERT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);

  // Single-stepping is the lowest priority interrupt source at an instruction
  // boundary, so the hardware interrupt takes its place rather than both
  // firing in one tick.
  EXPECT_EQ(helper->cpu_.registers[kCS], 0x0010);
  EXPECT_EQ(helper->cpu_.registers[kIP], 0x0100);
  EXPECT_FALSE(InSingleStepHandler(helper.get()));
}
