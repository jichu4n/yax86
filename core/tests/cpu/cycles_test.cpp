// Tests for what an instruction costs in CPU clock cycles.
//
// The 8088's timing is dominated by its 8-bit data bus, so most of what these
// pin down is how much traffic an instruction puts on it. That makes them the
// only tests that can see a certain class of defect: an instruction which
// reads memory it has no reason to read leaves the registers and memory
// exactly right, so every architectural test passes, and only the cycle count
// says the bus cycle happened.

#include <gtest/gtest.h>

#include <string>

#include "./test_helpers.h"
#include "cpu.h"

using namespace std;

namespace {

// Where the tests point BX, and where they put the stack. Both are inside the
// helper's 4KB of memory and clear of the program at 0x100.
enum : uint16_t {
  kOperandAddress = 0x0200,
  kStackPointer = 0x0400,
};

// Assembles a single instruction, runs it, and returns what it cost.
uint16_t CycleCost(const string& name, const string& asm_code) {
  auto helper = CPUTestHelper::CreateWithProgram(name, asm_code);
  helper->cpu_.registers[kBX] = kOperandAddress;
  helper->cpu_.registers[kSP] = kStackPointer;
  EXPECT_EQ(CPUTick(&helper->cpu_), kCPUTickExecuted);
  return helper->cpu_.cycles_this_tick;
}

}  // namespace

class CyclesTest : public ::testing::Test {};

// A register to register move touches neither memory nor the address adder, so
// it costs its base figure and nothing else.
TEST_F(CyclesTest, RegisterOperandsCostNoBusTime) {
  EXPECT_EQ(CycleCost("mov_reg_reg", "mov bx, ax"), 2);
  EXPECT_EQ(CycleCost("add_reg_reg", "add bx, ax"), 3);
}

// Base 2, plus 5 to compute [BX], plus 4 per byte read.
TEST_F(CyclesTest, LoadCostsOneMemoryAccess) {
  EXPECT_EQ(CycleCost("mov_al_mem", "mov al, [bx]"), 11);
  EXPECT_EQ(CycleCost("mov_ax_mem", "mov ax, [bx]"), 15);
}

// A store writes its destination and does not read it first, so it costs the
// same as the load above rather than twice the bus time. This is the case that
// no architectural test can distinguish.
TEST_F(CyclesTest, StoreCostsOneMemoryAccess) {
  EXPECT_EQ(CycleCost("mov_mem_al", "mov [bx], al"), 11);
  EXPECT_EQ(CycleCost("mov_mem_ax", "mov [bx], ax"), 15);
  EXPECT_EQ(CycleCost("mov_mem_imm8", "mov byte [bx], 1"), 11);
  EXPECT_EQ(CycleCost("mov_mem_imm16", "mov word [bx], 1"), 15);
  EXPECT_EQ(CycleCost("mov_mem_sreg", "mov [bx], ds"), 15);
}

// POP r/m16 reads the word off the stack and writes it to the destination,
// which is two accesses - but not the three it would be if the destination
// were read as well. Base 0, plus 5 for [BX], plus 8 twice.
TEST_F(CyclesTest, PopToMemoryCostsTwoMemoryAccesses) {
  EXPECT_EQ(CycleCost("pop_mem", "pop word [bx]"), 21);
}

// A read-modify-write genuinely does both, and is charged for both. These are
// the counterpart to the stores above: the same shape of instruction, where
// two accesses is the right answer.
TEST_F(CyclesTest, ReadModifyWriteCostsTwoMemoryAccesses) {
  EXPECT_EQ(CycleCost("add_mem_ax", "add [bx], ax"), 24);
  EXPECT_EQ(CycleCost("xchg_mem_ax", "xchg [bx], ax"), 25);
}

// The address adder is charged once per instruction, from the addressing mode
// alone, so a more expensive mode costs the same extra whatever the operation.
TEST_F(CyclesTest, AddressingModeIsChargedOncePerInstruction) {
  // [BX] is 5, [BX+2] is 9, and a bare displacement is 6. The last uses BX
  // rather than AX as the source because a store of AX to a bare address
  // assembles as MOV moffs16, AX, which carries no ModR/M byte and so pays
  // nothing for the address at all.
  EXPECT_EQ(CycleCost("mov_mem_disp_ax", "mov [bx+2], ax"), 19);
  EXPECT_EQ(CycleCost("mov_mem_abs_bx", "mov [0x200], bx"), 16);
  EXPECT_EQ(CycleCost("mov_moffs_ax", "mov [0x200], ax"), 10);
}
