// filepath: /home/chuan/Projects/yax86/tests/group_1_test.cpp
#include <gtest/gtest.h>

#include "../cpu.h"
#include "./test_helpers.h"

using namespace std;

class Group1Test : public ::testing::Test {};

TEST_F(Group1Test, AddImmediateByteToMemoryByte) {
  // Test case for ADD r/m8, imm8 (Opcode 80 /0 ib)
  // Example: ADD byte [bx], 0x12
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-add-rm8-imm8-test", "add byte [bx], 0x12\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;  // Point BX to some memory location
  helper->memory_[0x0800] = 0x01;        // Initial value in memory

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->memory_[0x0800], 0x13);  // 0x01 + 0x12 = 0x13
}

TEST_F(Group1Test, AddImmediateWordToMemoryWord) {
  // Test case for ADD r/m16, imm16 (Opcode 81 /0 iw)
  // Example: ADD word [bx], 0x1234
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-add-rm16-imm16-test", "add word [bx], 0x1234\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;  // Point BX to some memory location
  helper->memory_[0x0800] = 0x01;        // Low byte of initial value
  helper->memory_[0x0801] = 0x00;        // High byte of initial value (0x0001)

  helper->ExecuteInstructions(1);

  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x1235);  // 0x0001 + 0x1234 = 0x1235
}

TEST_F(Group1Test, AddImmediateByteSignExtendedToMemoryWord) {
  // Test case for ADD r/m16, imm8 (sign-extended) (Opcode 83 /0 ib)
  // Example: ADD word [bx], 0x12
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-add-rm16-imm8-test", "add word [bx], 0x12\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;  // Point BX to some memory location
  helper->memory_[0x0800] = 0x01;        // Low byte of initial value
  helper->memory_[0x0801] = 0x00;        // High byte of initial value (0x0001)

  helper->ExecuteInstructions(1);

  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0013);  // 0x0001 + 0x0012 (0x12 sign-extended) = 0x0013
}

// OR Instructions
TEST_F(Group1Test, OrImmediateByteWithMemoryByte) {
  // Opcode 80 /1 ib
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-or-rm8-imm8-test", "or byte [bx], 0x0F\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xF0;  // 11110000b

  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->memory_[0x0800], 0xFF);  // 11110000b | 00001111b = 11111111b
}

TEST_F(Group1Test, OrImmediateWordWithMemoryWord) {
  // Opcode 81 /1 iw
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-or-rm16-imm16-test", "or word [bx], 0x0F0F\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xF0;  // Low byte
  helper->memory_[0x0801] = 0xF0;  // High byte (0xF0F0)

  helper->ExecuteInstructions(1);
  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0xFFFF);  // 0xF0F0 | 0x0F0F = 0xFFFF
}

TEST_F(Group1Test, OrImmediateByteSignExtendedWithMemoryWord) {
  // Opcode 83 /1 ib
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-or-rm16-imm8-test", "or word [bx], 0x0F\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xF0;  // Low byte
  helper->memory_[0x0801] = 0x00;  // High byte (0x00F0)
                                   // Immediate 0x0F sign-extended is 0x000F
  helper->ExecuteInstructions(1);
  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x00FF);  // 0x00F0 | 0x000F = 0x00FF
}

// ADC Instructions
TEST_F(Group1Test, AddWithCarryImmediateByteToMemoryByte) {
  // Opcode 80 /2 ib
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-adc-rm8-imm8-test", "adc byte [bx], 0x01\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xFE;     // 254
  SetFlag(&helper->cpu_, kCF, true);  // Set Carry Flag

  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->memory_[0x0800],
      0x00);  // 254 + 1 + 1 (CF) = 256 -> 0 (with carry)
}

TEST_F(Group1Test, AddWithCarryImmediateWordToMemoryWord) {
  // Opcode 81 /2 iw
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-adc-rm16-imm16-test", "adc word [bx], 0x0001\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xFF;  // Low byte
  helper->memory_[0x0801] = 0xFF;  // High byte (0xFFFF)
  SetFlag(&helper->cpu_, kCF, true);

  helper->ExecuteInstructions(1);
  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0001);  // 0xFFFF + 0x0001 + 1 (CF) = 0x0001 (with carry)
}

TEST_F(Group1Test, AddWithCarryImmediateByteSignExtendedToMemoryWord) {
  // Opcode 83 /2 ib
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-adc-rm16-imm8-test", "adc word [bx], 0x01\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xFE;  // Low byte
  helper->memory_[0x0801] = 0xFF;  // High byte (0xFFFE)
  SetFlag(&helper->cpu_, kCF, true);
  // Immediate 0x01 sign-extended is 0x0001
  helper->ExecuteInstructions(1);
  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0000);  // 0xFFFE + 0x0001 + 1 (CF) = 0x0000 (with carry)
}

// SBB Instructions
TEST_F(Group1Test, SubtractWithBorrowImmediateByteFromMemoryByte) {
  // Opcode 80 /3 ib
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-sbb-rm8-imm8-test", "sbb byte [bx], 0x01\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x02;
  SetFlag(&helper->cpu_, kCF, true);  // Set Carry Flag (as borrow)

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x00);  // 0x02 - 0x01 - 1 (CF) = 0x00
}

TEST_F(Group1Test, SubtractWithBorrowImmediateWordFromMemoryWord) {
  // Opcode 81 /3 iw
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-sbb-rm16-imm16-test", "sbb word [bx], 0x0001\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x02;  // Low byte
  helper->memory_[0x0801] = 0x00;  // High byte (0x0002)
  SetFlag(&helper->cpu_, kCF, true);

  helper->ExecuteInstructions(1);
  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0000);  // 0x0002 - 0x0001 - 1 (CF) = 0x0000
}

TEST_F(Group1Test, SubtractWithBorrowImmediateByteSignExtendedFromMemoryWord) {
  // Opcode 83 /3 ib
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-sbb-rm16-imm8-test", "sbb word [bx], 0x01\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x02;  // Low byte
  helper->memory_[0x0801] = 0x00;  // High byte (0x0002)
  SetFlag(&helper->cpu_, kCF, true);
  // Immediate 0x01 sign-extended is 0x0001
  helper->ExecuteInstructions(1);
  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0000);  // 0x0002 - 0x0001 - 1 (CF) = 0x0000
}

// AND Instructions
TEST_F(Group1Test, AndImmediateByteWithMemoryByte) {
  // Opcode 80 /4 ib
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-and-rm8-imm8-test", "and byte [bx], 0x0F\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x3A;  // 00111010b

  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->memory_[0x0800], 0x0A);  // 00111010b & 00001111b = 00001010b
}

TEST_F(Group1Test, AndImmediateWordWithMemoryWord) {
  // Opcode 81 /4 iw
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-and-rm16-imm16-test", "and word [bx], 0x00FF\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x34;  // Low byte
  helper->memory_[0x0801] = 0x12;  // High byte (0x1234)

  helper->ExecuteInstructions(1);
  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0034);  // 0x1234 & 0x00FF = 0x0034
}

TEST_F(Group1Test, AndImmediateByteSignExtendedWithMemoryWord) {
  // Opcode 83 /4 ib
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-and-rm16-imm8-test",
      "and word [bx], 0x0F\n");  // Immediate 0x0F sign-extended is 0x000F
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x3A;  // Low byte
  helper->memory_[0x0801] = 0x12;  // High byte (0x123A)

  helper->ExecuteInstructions(1);
  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x000A);  // 0x123A & 0x000F = 0x000A
}

// SUB Instructions
TEST_F(Group1Test, SubtractImmediateByteFromMemoryByte) {
  // Opcode 80 /5 ib
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-sub-rm8-imm8-test", "sub byte [bx], 0x10\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x25;

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x15);  // 0x25 - 0x10 = 0x15
}

TEST_F(Group1Test, SubtractImmediateWordFromMemoryWord) {
  // Opcode 81 /5 iw
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-sub-rm16-imm16-test", "sub word [bx], 0x0110\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x25;  // Low byte
  helper->memory_[0x0801] = 0x02;  // High byte (0x0225)

  helper->ExecuteInstructions(1);
  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0115);  // 0x0225 - 0x0110 = 0x0115
}

TEST_F(Group1Test, SubtractImmediateByteSignExtendedFromMemoryWord) {
  // Opcode 83 /5 ib
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-sub-rm16-imm8-test",
      "sub word [bx], 0x10\n");  // Immediate 0x10 sign-extended is 0x0010
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x25;  // Low byte
  helper->memory_[0x0801] = 0x00;  // High byte (0x0025)

  helper->ExecuteInstructions(1);
  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0015);  // 0x0025 - 0x0010 = 0x0015
}

// XOR Instructions
TEST_F(Group1Test, XorImmediateByteWithMemoryByte) {
  // Opcode 80 /6 ib
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-xor-rm8-imm8-test", "xor byte [bx], 0xFF\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xAA;  // 10101010b

  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->memory_[0x0800], 0x55);  // 10101010b ^ 11111111b = 01010101b
}

TEST_F(Group1Test, XorImmediateWordWithMemoryWord) {
  // Opcode 81 /6 iw
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-xor-rm16-imm16-test", "xor word [bx], 0xFFFF\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xAA;  // Low byte
  helper->memory_[0x0801] = 0x55;  // High byte (0x55AA)

  helper->ExecuteInstructions(1);
  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0xAA55);  // 0x55AA ^ 0xFFFF = 0xAA55
}

TEST_F(Group1Test, XorImmediateByteSignExtendedWithMemoryWord) {
  // Opcode 83 /6 ib
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-xor-rm16-imm8-test",
      "xor word [bx], 0xFF\n");  // Immediate 0xFF sign-extended is 0x00FF
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xAA;  // Low byte
  helper->memory_[0x0801] = 0x55;  // High byte (0x55AA)

  helper->ExecuteInstructions(1);
  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x5555);  // 0x55AA ^ 0x00FF = 0x5555
}

// CMP Instructions
TEST_F(Group1Test, CompareImmediateByteWithMemoryByte) {
  // Opcode 80 /7 ib
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-cmp-rm8-imm8-test", "cmp byte [bx], 0x10\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x10;  // Value to compare

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x10);  // CMP does not change destination
  helper->CheckFlags({{kZF, true}});         // 0x10 - 0x10 = 0, so ZF is set
}

TEST_F(Group1Test, CompareImmediateWordWithMemoryWord) {
  // Opcode 81 /7 iw
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-cmp-rm16-imm16-test", "cmp word [bx], 0x1234\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x34;  // Low byte
  helper->memory_[0x0801] = 0x12;  // High byte (0x1234)

  helper->ExecuteInstructions(1);
  uint16_t value = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(value, 0x1234);
  helper->CheckFlags({{kZF, true}});
}

TEST_F(Group1Test, CompareImmediateByteSignExtendedWithMemoryWord) {
  // Opcode 83 /7 ib
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group1-cmp-rm16-imm8-test",
      "cmp word [bx], 0x34\n");  // Immediate 0x34 sign-extended is 0x0034
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x34;  // Low byte
  helper->memory_[0x0801] = 0x00;  // High byte (0x0034)

  helper->ExecuteInstructions(1);
  uint16_t value = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(value, 0x0034);
  helper->CheckFlags({{kZF, true}});
}
