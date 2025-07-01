#include <gtest/gtest.h>

#include "../cpu.h"
#include "./test_helpers.h"

using namespace std;

class PushPopTest : public ::testing::Test {};

TEST_F(PushPopTest, PushPopRegisters) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-push-pop-test",
      "push ax\n"   // Push AX onto the stack
      "push cx\n"   // Push CX onto the stack
      "pop dx\n"    // Pop from the stack into BX
      "pop bx\n");  // Pop from the stack into DX
  helper->cpu_.registers[kSS] = 0;
  helper->cpu_.registers[kSP] = helper->memory_size_ - 2;
  // Set up: AX=0x1234, CX=0x5678
  helper->cpu_.registers[kAX] = 0x1234;
  helper->cpu_.registers[kCX] = 0x5678;
  helper->cpu_.registers[kBX] = 0;
  helper->cpu_.registers[kDX] = 0;

  helper->ExecuteInstructions(4);
  EXPECT_EQ(helper->cpu_.registers[kBX], 0x1234);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x5678);
}

TEST_F(PushPopTest, PushPopSegmentRegisters) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-push-pop-segment-test",
      "push ds\n"   // Push DS onto the stack
      "push es\n"   // Push ES onto the stack
      "pop ds\n"    // Pop from the stack into SS
      "pop es\n");  // Pop from the stack into CS
  helper->cpu_.registers[kSS] = 0;
  helper->cpu_.registers[kSP] = helper->memory_size_ - 2;
  // Set up: DS=0x1234, ES=0x5678
  helper->cpu_.registers[kDS] = 0x1234;
  helper->cpu_.registers[kES] = 0x5678;

  helper->ExecuteInstructions(4);
  EXPECT_EQ(helper->cpu_.registers[kDS], 0x5678);
  EXPECT_EQ(helper->cpu_.registers[kES], 0x1234);
}

TEST_F(PushPopTest, PopRegister) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-pop-r16-test",
      "db 0x8f, 0xc0\n");  // POP AX
  helper->cpu_.registers[kSS] = 0;
  // Initial SP points to the end of a 2-byte value (0xABCD)
  uint16_t initial_sp = helper->memory_size_ - 4;
  helper->cpu_.registers[kSP] = initial_sp;

  // Push 0xABCD onto the stack manually for the POP instruction
  // Memory: [..., 0xCD, 0xAB]
  // SP will point to 0xCD after POP
  helper->memory_[initial_sp] = 0xCD;      // Low byte
  helper->memory_[initial_sp + 1] = 0xAB;  // High byte

  // Set up: AX = 0 initially
  helper->cpu_.registers[kAX] = 0;

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0xABCD);
  EXPECT_EQ(helper->cpu_.registers[kSP], initial_sp + 2);
}

TEST_F(PushPopTest, PopMemory) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-pop-m16-test", "pop word [bx-2]\n");
  helper->cpu_.registers[kSS] = 0;
  // Initial SP points to the end of a 2-byte value (0xABCD)
  uint16_t initial_sp = helper->memory_size_ - 4;
  helper->cpu_.registers[kSP] = initial_sp;

  // Push 0xABCD onto the stack manually for the POP instruction
  // Memory: [..., 0xCD, 0xAB]
  // SP will point to 0xCD after POP
  helper->memory_[initial_sp] = 0xCD;      // Low byte
  helper->memory_[initial_sp + 1] = 0xAB;  // High byte

  helper->cpu_.registers[kBX] = 0x0402;
  helper->memory_[0x400] = 0;
  helper->memory_[0x401] = 0;  // High byte

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x400], 0xCD);  // Low byte
  EXPECT_EQ(helper->memory_[0x401], 0xAB);  // High byte
  EXPECT_EQ(helper->cpu_.registers[kSP], initial_sp + 2);
}
