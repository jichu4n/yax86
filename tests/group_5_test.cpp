// filepath: /home/chuan/Projects/yax86/tests/group_5_test.cpp
#include <gtest/gtest.h>

#include "../cpu.h"
#include "./test_helpers.h"

using namespace std;

class Group5Test : public ::testing::Test {};

TEST_F(Group5Test, IncMemoryWord) {
  // Test case for INC r/m16 (Opcode FF /0)
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group5-inc-rm16-test", "inc word [bx]");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x01;
  helper->memory_[0x0801] = 0x00;

  helper->ExecuteInstructions(1);

  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0002);
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kOF, false}, {kAF, false}});

  // Test with overflow
  helper = CPUTestHelper::CreateWithProgram(
      "execute-group5-inc-rm16-overflow-test", "inc word [bx]");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xFF;
  helper->memory_[0x0801] = 0x7F;

  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x8000);  // Should be -32768
  helper->CheckFlags({{kZF, false}, {kSF, true}, {kOF, true}, {kAF, true}});

  // Test with zero result
  helper = CPUTestHelper::CreateWithProgram(
      "execute-group5-inc-rm16-zero-test", "inc word [bx]");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xFF;
  helper->memory_[0x0801] = 0xFF;

  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0000);
  helper->CheckFlags({{kZF, true}, {kSF, false}, {kOF, false}, {kAF, true}});
}

TEST_F(Group5Test, DecMemoryWord) {
  // Test case for DEC r/m16 (Opcode FF /1)
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group5-dec-rm16-test", "dec word [bx]");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x02;
  helper->memory_[0x0801] = 0x00;

  helper->ExecuteInstructions(1);

  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0001);
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kOF, false}, {kAF, false}});

  // Test with overflow (to positive from min negative)
  helper = CPUTestHelper::CreateWithProgram(
      "execute-group5-dec-rm16-overflow-test", "dec word [bx]");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x00;
  helper->memory_[0x0801] = 0x80;

  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x7FFF);  // Should be 32767 (max positive)
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kOF, true}, {kAF, true}});

  // Test with zero result
  helper = CPUTestHelper::CreateWithProgram(
      "execute-group5-dec-rm16-zero-test", "dec word [bx]");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x01;
  helper->memory_[0x0801] = 0x00;

  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0000);
  helper->CheckFlags({{kZF, true}, {kSF, false}, {kOF, false}, {kAF, false}});
}

TEST_F(Group5Test, CallIndirectNear) {
  // Test case for CALL r/m16 (Opcode FF /2)
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group5-call-indirect-near-test", "call word [bx + 2]");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kSP] = helper->memory_size_ - 2;
  helper->cpu_.registers[kBX] = 0x07FE;
  helper->memory_[0x0800] = 0x34;
  helper->memory_[0x0801] = 0x12;

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kSP], helper->memory_size_ - 4);
  uint16_t return_address = helper->memory_[helper->memory_size_ - 4] |
                            (helper->memory_[helper->memory_size_ - 3] << 8);
  EXPECT_EQ(return_address, kCOMFileLoadOffset + 3);
  EXPECT_EQ(helper->cpu_.registers[kIP], 0x1234);
}

TEST_F(Group5Test, CallIndirectFar) {
  // Test case for CALL m16:16 (Opcode FF /3)
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group5-call-indirect-far-test", "call far [bx+2]");
  helper->cpu_.registers[kDS] = 0;  // DS for memory access
  helper->cpu_.registers[kSP] = helper->memory_size_ - 2;
  helper->cpu_.registers[kBX] = 0x07FE;
  helper->memory_[0x0800] = 0x34;
  helper->memory_[0x0801] = 0x12;
  helper->memory_[0x0802] = 0x00;
  helper->memory_[0x0803] = 0x0F;

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kCS], 0x0F00);
  EXPECT_EQ(helper->cpu_.registers[kIP], 0x1234);
  EXPECT_EQ(helper->cpu_.registers[kSP], helper->memory_size_ - 6);
  uint16_t pushed_cs = helper->memory_[helper->memory_size_ - 4] |
                       (helper->memory_[helper->memory_size_ - 3] << 8);
  EXPECT_EQ(pushed_cs, 0);

  uint16_t pushed_ip = helper->memory_[helper->memory_size_ - 6] |
                       (helper->memory_[helper->memory_size_ - 5] << 8);
  EXPECT_EQ(pushed_ip, kCOMFileLoadOffset + 3);
}

TEST_F(Group5Test, JmpIndirectNear) {
  // Test case for JMP r/m16 (Opcode FF /4)
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group5-jmp-indirect-near-test", "jmp word [bx-4]");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0804;
  helper->memory_[0x0800] = 0x34;
  helper->memory_[0x0801] = 0x12;

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kIP], 0x1234);
}

TEST_F(Group5Test, JmpIndirectFar) {
  // Test case for JMP m16:16 (Opcode FF /5)
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group5-jmp-indirect-far-test", "jmp far [bx]");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x00;
  helper->memory_[0x0801] = 0x10;
  helper->memory_[0x0802] = 0x00;
  helper->memory_[0x0803] = 0x0F;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kCS], 0x0F00);
  EXPECT_EQ(helper->cpu_.registers[kIP], 0x1000);
}

TEST_F(Group5Test, PushIndirect) {
  // Test case for PUSH r/m16 (Opcode FF /6)
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group5-push-indirect-test", "push word [bx]");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kSP] = helper->memory_size_ - 2;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xCD;
  helper->memory_[0x0801] = 0xAB;

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kSP], helper->memory_size_ - 4);
  uint16_t pushed_value = helper->memory_[helper->memory_size_ - 4] |
                          (helper->memory_[helper->memory_size_ - 3] << 8);
  EXPECT_EQ(pushed_value, 0xABCD);
}
