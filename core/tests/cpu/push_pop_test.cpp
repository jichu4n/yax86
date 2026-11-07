#include <gtest/gtest.h>

#include "./test_helpers.h"
#include "cpu.h"

using namespace std;

class PushPopTest : public ::testing::Test {};

TEST_F(PushPopTest, PushPopRegisters) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-push-pop-test",
      "push ax\n"   // Push AX onto the stack
      "push cx\n"   // Push CX onto the stack
      "pop dx\n"    // Pop from the stack into BX
      "pop bx\n");  // Pop from the stack into DX
  helper->cpu_.registers[Y_CPU_SS] = 0;
  helper->cpu_.registers[Y_CPU_SP] = helper->memory_size_ - 2;
  // Set up: AX=0x1234, CX=0x5678
  helper->cpu_.registers[Y_CPU_AX] = 0x1234;
  helper->cpu_.registers[Y_CPU_CX] = 0x5678;
  helper->cpu_.registers[Y_CPU_BX] = 0;
  helper->cpu_.registers[Y_CPU_DX] = 0;

  helper->ExecuteInstructions(4);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_BX], 0x1234);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_DX], 0x5678);
}

TEST_F(PushPopTest, PushPopSegmentRegisters) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-push-pop-segment-test",
      "push ds\n"   // Push DS onto the stack
      "push es\n"   // Push ES onto the stack
      "pop ds\n"    // Pop from the stack into SS
      "pop es\n");  // Pop from the stack into CS
  helper->cpu_.registers[Y_CPU_SS] = 0;
  helper->cpu_.registers[Y_CPU_SP] = helper->memory_size_ - 2;
  // Set up: DS=0x1234, ES=0x5678
  helper->cpu_.registers[Y_CPU_DS] = 0x1234;
  helper->cpu_.registers[Y_CPU_ES] = 0x5678;

  helper->ExecuteInstructions(4);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_DS], 0x5678);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_ES], 0x1234);
}

TEST_F(PushPopTest, PopRegister) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-pop-r16-test",
      "db 0x8f, 0xc0\n");  // POP AX
  helper->cpu_.registers[Y_CPU_SS] = 0;
  // Initial SP points to the end of a 2-byte value (0xABCD)
  uint16_t initial_sp = helper->memory_size_ - 4;
  helper->cpu_.registers[Y_CPU_SP] = initial_sp;

  // Push 0xABCD onto the stack manually for the POP instruction
  // Memory: [..., 0xCD, 0xAB]
  // SP will point to 0xCD after POP
  helper->memory_[initial_sp] = 0xCD;      // Low byte
  helper->memory_[initial_sp + 1] = 0xAB;  // High byte

  // Set up: AX = 0 initially
  helper->cpu_.registers[Y_CPU_AX] = 0;

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_AX], 0xABCD);
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_SP], initial_sp + 2);
}

TEST_F(PushPopTest, PopMemory) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-pop-m16-test", "pop word [bx-2]\n");
  helper->cpu_.registers[Y_CPU_SS] = 0;
  // Initial SP points to the end of a 2-byte value (0xABCD)
  uint16_t initial_sp = helper->memory_size_ - 4;
  helper->cpu_.registers[Y_CPU_SP] = initial_sp;

  // Push 0xABCD onto the stack manually for the POP instruction
  // Memory: [..., 0xCD, 0xAB]
  // SP will point to 0xCD after POP
  helper->memory_[initial_sp] = 0xCD;      // Low byte
  helper->memory_[initial_sp + 1] = 0xAB;  // High byte

  helper->cpu_.registers[Y_CPU_BX] = 0x0402;
  helper->memory_[0x400] = 0;
  helper->memory_[0x401] = 0;  // High byte

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x400], 0xCD);  // Low byte
  EXPECT_EQ(helper->memory_[0x401], 0xAB);  // High byte
  EXPECT_EQ(helper->cpu_.registers[Y_CPU_SP], initial_sp + 2);
}

// The 8086/8088 moves the stack pointer before it reads the source, so PUSH SP
// stores the value SP has after the decrement. The 80286 and later store the
// value SP had on entry, so this is one of the few ways to tell the two apart.
TEST_F(PushPopTest, PushStackPointerPushesTheDecrementedValue) {
  auto helper =
      CPUTestHelper::CreateWithProgram("execute-push-sp-test", "push sp\n");
  helper->cpu_.registers[Y_CPU_SS] = 0;
  const uint16_t initial_sp = 0x0800;
  helper->cpu_.registers[Y_CPU_SP] = initial_sp;

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[Y_CPU_SP], initial_sp - 2);
  EXPECT_EQ(helper->memory_[initial_sp - 2], (initial_sp - 2) & 0xFF);
  EXPECT_EQ(helper->memory_[initial_sp - 1], (initial_sp - 2) >> 8);
}

// The same holds for the r/m16 form, including its undocumented REG 7 alias.
TEST_F(PushPopTest, PushStackPointerViaGroup5PushesTheDecrementedValue) {
  for (uint8_t mod_rm : {0xF4, 0xFC}) {  // FF /6 and FF /7, both with rm = SP
    auto helper = make_unique<CPUTestHelper>();
    helper->LoadCOM({0xFF, mod_rm});
    helper->cpu_.registers[Y_CPU_SS] = 0;
    const uint16_t initial_sp = 0x0800;
    helper->cpu_.registers[Y_CPU_SP] = initial_sp;

    helper->ExecuteInstructions(1);

    EXPECT_EQ(helper->cpu_.registers[Y_CPU_SP], initial_sp - 2)
        << "mod_rm " << hex << static_cast<int>(mod_rm);
    EXPECT_EQ(helper->memory_[initial_sp - 2], (initial_sp - 2) & 0xFF)
        << "mod_rm " << hex << static_cast<int>(mod_rm);
  }
}

// A register other than SP is unaffected: its value is read as it stands.
TEST_F(PushPopTest, PushOtherRegisterIsUnaffectedByTheOrdering) {
  auto helper =
      CPUTestHelper::CreateWithProgram("execute-push-bp-test", "push bp\n");
  helper->cpu_.registers[Y_CPU_SS] = 0;
  helper->cpu_.registers[Y_CPU_SP] = 0x0800;
  helper->cpu_.registers[Y_CPU_BP] = 0x1234;

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->memory_[0x07FE], 0x34);
  EXPECT_EQ(helper->memory_[0x07FF], 0x12);
}
