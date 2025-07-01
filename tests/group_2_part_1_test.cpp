#include <gtest/gtest.h>

#include "../cpu.h"
#include "./test_helpers.h"

using namespace std;

class Group2Test : public ::testing::Test {};

TEST_F(Group2Test, ShlByte1) {
  // Test case for SHL r/m8, 1 (Opcode D0 /4)
  // Example: SHL byte [bx], 1
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-shl-byte-1-test", "shl byte [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;

  // Test 1: No carry, no overflow
  helper->memory_[0x0800] = 0x40;  // 01000000b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x80);  // 10000000b
  helper->CheckFlags(
      {{kZF, false}, {kSF, true}, {kPF, false}, {kCF, false}, {kOF, true}});

  // Test 2: Carry generated
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shl-byte-1-carry-test", "shl byte [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x80;  // 10000000b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x00);  // 00000000b
  helper->CheckFlags(
      {{kZF, true}, {kSF, false}, {kPF, true}, {kCF, true}, {kOF, true}});

  // Test 3: Multiple bits set
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shl-byte-1-multiple-test", "shl byte [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x55;  // 01010101b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0xAA);  // 10101010b
  helper->CheckFlags(
      {{kZF, false}, {kSF, true}, {kPF, true}, {kCF, false}, {kOF, true}});
}

TEST_F(Group2Test, ShlWord1) {
  // Test case for SHL r/m16, 1 (Opcode D1 /4)
  // Example: SHL word [bx], 1
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-shl-word-1-test", "shl word [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;

  // Test 1: No carry, no overflow
  helper->memory_[0x0800] = 0x00;  // Low byte
  helper->memory_[0x0801] = 0x40;  // High byte (0x4000)
  helper->ExecuteInstructions(1);
  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x8000);
  helper->CheckFlags(
      {{kZF, false}, {kSF, true}, {kPF, true}, {kCF, false}, {kOF, true}});

  // Test 2: Carry generated
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shl-word-1-carry-test", "shl word [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x00;  // Low byte
  helper->memory_[0x0801] = 0x80;  // High byte (0x8000)
  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0000);
  helper->CheckFlags(
      {{kZF, true}, {kSF, false}, {kPF, true}, {kCF, true}, {kOF, true}});

  // Test 3: Multiple bits set
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shl-word-1-multiple-test", "shl word [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x55;  // Low byte
  helper->memory_[0x0801] = 0x55;  // High byte (0x5555)
  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0xAAAA);
  helper->CheckFlags(
      {{kZF, false}, {kSF, true}, {kPF, true}, {kCF, false}, {kOF, true}});
}

TEST_F(Group2Test, ShlByteCL) {
  // Test case for SHL r/m8, CL (Opcode D2 /4)
  // Example: SHL byte [bx], cl
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-shl-byte-cl-test", "shl byte [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;

  // Test 1: Shift by 0 (no change, no flags affected)
  helper->cpu_.registers[kCX] = 0x0000;  // CL = 0
  helper->memory_[0x0800] = 0x55;
  SetFlag(&helper->cpu_, kCF, true);  // Set carry to verify it's unchanged
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x55);
  helper->CheckFlags({{kCF, true}});  // CF should remain unchanged

  // Test 2: Shift by 2
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shl-byte-cl-2-test", "shl byte [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0002;  // CL = 2
  helper->memory_[0x0800] = 0x15;        // 00010101b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x54);  // 01010100b
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, false}, {kCF, false}});

  // Test 3: Shift by 3 with carry
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shl-byte-cl-3-test", "shl byte [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0003;  // CL = 3
  helper->memory_[0x0800] = 0x21;        // 00100001b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x08);  // 00001000b
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, false}, {kCF, true}});

  // Test 4: Shift by 7 (maximum without wrapping)
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shl-byte-cl-7-test", "shl byte [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0007;  // CL = 7
  helper->memory_[0x0800] = 0x01;        // 00000001b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x80);  // 10000000b
  helper->CheckFlags({{kZF, false}, {kSF, true}, {kPF, false}, {kCF, false}});

  // Test 5: Shift by 8 (result should be 0)
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shl-byte-cl-8-test", "shl byte [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0008;  // CL = 8
  helper->memory_[0x0800] = 0xFF;        // 11111111b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x00);  // 00000000b
  helper->CheckFlags({{kZF, true}, {kSF, false}, {kPF, true}, {kCF, true}});
}

TEST_F(Group2Test, ShlWordCL) {
  // Test case for SHL r/m16, CL (Opcode D3 /4)
  // Example: SHL word [bx], cl
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-shl-word-cl-test", "shl word [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;

  // Test 1: Shift by 0 (no change, no flags affected)
  helper->cpu_.registers[kCX] = 0x0000;  // CL = 0
  helper->memory_[0x0800] = 0x55;        // Low byte
  helper->memory_[0x0801] = 0xAA;        // High byte (0xAA55)
  SetFlag(&helper->cpu_, kCF, true);     // Set carry to verify it's unchanged
  helper->ExecuteInstructions(1);
  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0xAA55);
  helper->CheckFlags({{kCF, true}});  // CF should remain unchanged

  // Test 2: Shift by 4
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shl-word-cl-4-test", "shl word [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0004;  // CL = 4
  helper->memory_[0x0800] = 0x34;        // Low byte
  helper->memory_[0x0801] = 0x12;        // High byte (0x1234)
  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x2340);  // 0x1234 << 4 = 0x2340
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, false}, {kCF, true}});

  // Test 3: Shift by 8
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shl-word-cl-8-test", "shl word [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0008;  // CL = 8
  helper->memory_[0x0800] = 0x34;        // Low byte
  helper->memory_[0x0801] = 0x12;        // High byte (0x1234)
  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x3400);
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, true}, {kCF, false}});

  // Test 4: Shift by 16 (result should be 0)
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shl-word-cl-16-test", "shl word [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0010;  // CL = 16
  helper->memory_[0x0800] = 0xFF;        // Low byte
  helper->memory_[0x0801] = 0xFF;        // High byte (0xFFFF)
  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0000);
  helper->CheckFlags({{kZF, true}, {kSF, false}, {kPF, true}, {kCF, true}});
}

TEST_F(Group2Test, ShlRegisterByte) {
  // Test case for SHL r8, 1 via ModR/M encoding
  // Example: SHL AL, 1
  auto helper =
      CPUTestHelper::CreateWithProgram("group2-shl-al-1-test", "shl al, 1\n");

  helper->cpu_.registers[kAX] = 0x1242;  // AL = 0x42
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x84);         // AL = 0x84
  EXPECT_EQ((helper->cpu_.registers[kAX] >> 8) & 0xFF, 0x12);  // AH unchanged
  helper->CheckFlags(
      {{kZF, false}, {kSF, true}, {kPF, true}, {kCF, false}, {kOF, true}});

  // Test with BH register
  helper =
      CPUTestHelper::CreateWithProgram("group2-shl-bh-1-test", "shl bh, 1\n");
  helper->cpu_.registers[kBX] = 0x4078;  // BH = 0x40
  helper->ExecuteInstructions(1);
  EXPECT_EQ((helper->cpu_.registers[kBX] >> 8) & 0xFF, 0x80);  // BH = 0x80
  EXPECT_EQ(helper->cpu_.registers[kBX] & 0xFF, 0x78);         // BL unchanged
  helper->CheckFlags(
      {{kZF, false}, {kSF, true}, {kPF, false}, {kCF, false}, {kOF, true}});
}

TEST_F(Group2Test, ShlRegisterWord) {
  // Test case for SHL r16, 1 via ModR/M encoding
  // Example: SHL AX, 1
  auto helper =
      CPUTestHelper::CreateWithProgram("group2-shl-ax-1-test", "shl ax, 1\n");

  helper->cpu_.registers[kAX] = 0x4234;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x8468);
  helper->CheckFlags(
      {{kZF, false}, {kSF, true}, {kPF, false}, {kCF, false}, {kOF, true}});

  // Test with CX register and CL count
  helper =
      CPUTestHelper::CreateWithProgram("group2-shl-cx-cl-test", "shl cx, cl\n");
  helper->cpu_.registers[kCX] = 0x1234;  // CH = 0x12, CL = 0x34
  // Note: CL is used as the shift count, so CL = 0x34 = 52.
  // For the 8086, the lower 5 bits of CL are used, so the effective count is
  // 52 & 0x1F = 20.
  helper->ExecuteInstructions(1);
  // Since the shift count (20) is > 16, the result is 0. The last bit
  // shifted out determines the Carry Flag. After 16 shifts, all original bits
  // are gone, so any subsequent shifts will shift out zeros. Thus, CF is 0.
  EXPECT_EQ(helper->cpu_.registers[kCX], 0x0000);
  helper->CheckFlags({{kZF, true}, {kSF, false}, {kPF, true}, {kCF, false}});
}

TEST_F(Group2Test, ShlMemoryWithDisplacement) {
  // Test case for SHL with memory operand using displacement
  // Example: SHL byte [bx+2], 1
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-shl-displacement-test", "shl byte [bx+2], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0802] = 0x15;  // 00010101b

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0802], 0x2A);  // 00101010b
  helper->CheckFlags(
      {{kZF, false}, {kSF, false}, {kPF, false}, {kCF, false}, {kOF, false}});
}

TEST_F(Group2Test, ShlOverflowFlag) {
  // Test specific cases for overflow flag behavior
  // OF is set only for 1-bit shifts and when the sign bit changes

  // Test 1: No overflow (sign bit doesn't change)
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-shl-no-overflow-test", "shl al, 1\n");
  helper->cpu_.registers[kAX] = 0x0030;  // AL = 0x30 (00110000b)
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x60);  // AL = 0x60 (01100000b)
  helper->CheckFlags({{kOF, false}});  // No overflow, sign bit stayed 0

  // Test 2: Overflow (sign bit changes from 0 to 1)
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shl-overflow-0to1-test", "shl al, 1\n");
  helper->cpu_.registers[kAX] = 0x0040;  // AL = 0x40 (01000000b)
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x80);  // AL = 0x80 (10000000b)
  helper->CheckFlags({{kOF, true}});  // Overflow, sign bit changed from 0 to 1

  // Test 3: No overflow, but carry is set (sign bit does not change)
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shl-no-overflow-carry-set-test", "shl al, 1");
  helper->cpu_.registers[kAX] = 0x00C0;  // AL = 0xC0 (11000000b)
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x80);  // AL = 0x80 (10000000b)
  helper->CheckFlags(
      {{kSF, true}, {kZF, false}, {kPF, false}, {kCF, true}, {kOF, false}});
}

TEST_F(Group2Test, ShrByte1) {
  // Test case for SHR r/m8, 1 (Opcode D0 /5)
  // Example: SHR byte [bx], 1
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-shr-byte-1-test", "shr byte [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;

  // Test 1: No carry, overflow
  helper->memory_[0x0800] = 0x80;  // 10000000b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x40);  // 01000000b
  helper->CheckFlags(
      {{kZF, false}, {kSF, false}, {kPF, false}, {kCF, false}, {kOF, true}});

  // Test 2: Carry generated
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shr-byte-1-carry-test", "shr byte [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x01;  // 00000001b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x00);  // 00000000b
  helper->CheckFlags(
      {{kZF, true}, {kSF, false}, {kPF, true}, {kCF, true}, {kOF, false}});

  // Test 3: Multiple bits set
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shr-byte-1-multiple-test", "shr byte [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xAA;  // 10101010b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x55);  // 01010101b
  helper->CheckFlags(
      {{kZF, false}, {kSF, false}, {kPF, true}, {kCF, false}, {kOF, true}});
}

TEST_F(Group2Test, ShrWord1) {
  // Test case for SHR r/m16, 1 (Opcode D1 /5)
  // Example: SHR word [bx], 1
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-shr-word-1-test", "shr word [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;

  // Test 1: No carry, overflow
  helper->memory_[0x0800] = 0x00;  // Low byte
  helper->memory_[0x0801] = 0x80;  // High byte (0x8000)
  helper->ExecuteInstructions(1);
  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x4000);
  helper->CheckFlags(
      {{kZF, false}, {kSF, false}, {kPF, true}, {kCF, false}, {kOF, true}});

  // Test 2: Carry generated
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shr-word-1-carry-test", "shr word [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x01;  // Low byte
  helper->memory_[0x0801] = 0x00;  // High byte (0x0001)
  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0000);
  helper->CheckFlags(
      {{kZF, true}, {kSF, false}, {kPF, true}, {kCF, true}, {kOF, false}});

  // Test 3: Multiple bits set
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shr-word-1-multiple-test", "shr word [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xAA;  // Low byte
  helper->memory_[0x0801] = 0xAA;  // High byte (0xAAAA)
  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x5555);
  helper->CheckFlags(
      {{kZF, false}, {kSF, false}, {kPF, true}, {kCF, false}, {kOF, true}});
}

TEST_F(Group2Test, ShrByteCL) {
  // Test case for SHR r/m8, CL (Opcode D2 /5)
  // Example: SHR byte [bx], cl
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-shr-byte-cl-test", "shr byte [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;

  // Test 1: Shift by 0 (no change, no flags affected)
  helper->cpu_.registers[kCX] = 0x0000;  // CL = 0
  helper->memory_[0x0800] = 0x55;
  SetFlag(&helper->cpu_, kCF, true);  // Set carry to verify it's unchanged
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x55);
  helper->CheckFlags({{kCF, true}});  // CF should remain unchanged

  // Test 2: Shift by 2
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shr-byte-cl-2-test", "shr byte [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0002;  // CL = 2
  helper->memory_[0x0800] = 0x54;        // 01010100b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x15);  // 00010101b
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, false}, {kCF, false}});

  // Test 3: Shift by 3 with carry
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shr-byte-cl-3-test", "shr byte [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0003;  // CL = 3
  helper->memory_[0x0800] = 0x8A;        // 10001010b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x11);  // 00010001b
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, true}, {kCF, false}});

  // Test 4: Shift by 8 (result should be 0)
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shr-byte-cl-8-test", "shr byte [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0008;  // CL = 8
  helper->memory_[0x0800] = 0xFF;        // 11111111b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x00);  // 00000000b
  helper->CheckFlags({{kZF, true}, {kSF, false}, {kPF, true}, {kCF, true}});
}

TEST_F(Group2Test, ShrWordCL) {
  // Test case for SHR r/m16, CL (Opcode D3 /5)
  // Example: SHR word [bx], cl
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-shr-word-cl-test", "shr word [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;

  // Test 1: Shift by 0 (no change, no flags affected)
  helper->cpu_.registers[kCX] = 0x0000;  // CL = 0
  helper->memory_[0x0800] = 0x55;        // Low byte
  helper->memory_[0x0801] = 0xAA;        // High byte (0xAA55)
  SetFlag(&helper->cpu_, kCF, true);     // Set carry to verify it's unchanged
  helper->ExecuteInstructions(1);
  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0xAA55);
  helper->CheckFlags({{kCF, true}});  // CF should remain unchanged

  // Test 2: Shift by 4
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shr-word-cl-4-test", "shr word [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0004;  // CL = 4
  helper->memory_[0x0800] = 0x34;        // Low byte
  helper->memory_[0x0801] = 0x12;        // High byte (0x1234)
  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0123);  // 0x1234 >> 4 = 0x0123
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, false}, {kCF, false}});

  // Test 3: Shift by 8
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shr-word-cl-8-test", "shr word [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0008;  // CL = 8
  helper->memory_[0x0800] = 0x34;        // Low byte
  helper->memory_[0x0801] = 0x12;        // High byte (0x1234)
  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0012);
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, true}, {kCF, false}});

  // Test 4: Shift by 16 (result should be 0)
  helper = CPUTestHelper::CreateWithProgram(
      "group2-shr-word-cl-16-test", "shr word [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0010;  // CL = 16
  helper->memory_[0x0800] = 0xFF;        // Low byte
  helper->memory_[0x0801] = 0xFF;        // High byte (0xFFFF)
  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0000);
  helper->CheckFlags({{kZF, true}, {kSF, false}, {kPF, true}, {kCF, true}});
}

TEST_F(Group2Test, RolByte1) {
  // Test case for ROL r/m8, 1 (Opcode D0 /0)
  // Example: ROL byte [bx], 1
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-rol-byte-1-test", "rol byte [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;

  // Test 1: No carry, no overflow
  helper->memory_[0x0800] = 0x40;  // 01000000b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x80);  // 10000000b
  helper->CheckFlags({{kCF, false}, {kOF, true}});

  // Test 2: Carry generated (MSB rotates to LSB and CF)
  helper = CPUTestHelper::CreateWithProgram(
      "group2-rol-byte-1-carry-test", "rol byte [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x80;  // 10000000b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x01);  // 00000001b (MSB rotated to LSB)
  helper->CheckFlags({{kCF, true}, {kOF, true}});

  // Test 3: Multiple bits set
  helper = CPUTestHelper::CreateWithProgram(
      "group2-rol-byte-1-multiple-test", "rol byte [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xAA;  // 10101010b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x55);  // 01010101b (MSB rotated to LSB)
  helper->CheckFlags({{kCF, true}, {kOF, true}});

  // Test 4: All bits set
  helper = CPUTestHelper::CreateWithProgram(
      "group2-rol-byte-1-all-bits-test", "rol byte [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xFF;  // 11111111b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0xFF);  // 11111111b (no change)
  helper->CheckFlags({{kCF, true}, {kOF, false}});
}

TEST_F(Group2Test, RolWord1) {
  // Test case for ROL r/m16, 1 (Opcode D1 /0)
  // Example: ROL word [bx], 1
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-rol-word-1-test", "rol word [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;

  // Test 1: No carry, no overflow
  helper->memory_[0x0800] = 0x00;  // Low byte
  helper->memory_[0x0801] = 0x40;  // High byte (0x4000)
  helper->ExecuteInstructions(1);
  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x8000);
  helper->CheckFlags({{kCF, false}, {kOF, true}});

  // Test 2: Carry generated (MSB rotates to LSB and CF)
  helper = CPUTestHelper::CreateWithProgram(
      "group2-rol-word-1-carry-test", "rol word [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x00;  // Low byte
  helper->memory_[0x0801] = 0x80;  // High byte (0x8000)
  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0001);  // MSB rotated to LSB
  helper->CheckFlags({{kCF, true}, {kOF, true}});

  // Test 3: Multiple bits set
  helper = CPUTestHelper::CreateWithProgram(
      "group2-rol-word-1-multiple-test", "rol word [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xAA;  // Low byte
  helper->memory_[0x0801] = 0xAA;  // High byte (0xAAAA)
  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x5555);  // MSB rotated to LSB
  helper->CheckFlags({{kCF, true}, {kOF, true}});
}

TEST_F(Group2Test, RolByteCL) {
  // Test case for ROL r/m8, CL (Opcode D2 /0)
  // Example: ROL byte [bx], cl
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-rol-byte-cl-test", "rol byte [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;

  // Test 1: Rotate by 0 (no change, no flags affected)
  helper->cpu_.registers[kCX] = 0x0000;  // CL = 0
  helper->memory_[0x0800] = 0x55;
  SetFlag(&helper->cpu_, kCF, true);  // Set carry to verify it's unchanged
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x55);
  helper->CheckFlags({{kCF, true}});  // CF should remain unchanged

  // Test 2: Rotate by 2
  helper = CPUTestHelper::CreateWithProgram(
      "group2-rol-byte-cl-2-test", "rol byte [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0002;  // CL = 2
  helper->memory_[0x0800] = 0x15;        // 00010101b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x54);  // 01010100b
  helper->CheckFlags({{kCF, false}});

  // Test 3: Rotate by 3
  helper = CPUTestHelper::CreateWithProgram(
      "group2-rol-byte-cl-3-test", "rol byte [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0003;  // CL = 3
  helper->memory_[0x0800] = 0x21;        // 00100001b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->memory_[0x0800],
      0x09);  // 00001001b (rotated left by 3: 00001 + 001)
  helper->CheckFlags({{kCF, true}});

  // Test 4: Rotate by 4
  helper = CPUTestHelper::CreateWithProgram(
      "group2-rol-byte-cl-4-test", "rol byte [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0004;  // CL = 4
  helper->memory_[0x0800] = 0xF0;        // 11110000b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x0F);  // 00001111b
  helper->CheckFlags({{kCF, true}});

  // Test 5: Rotate by 8 (full rotation, should be same as original)
  helper = CPUTestHelper::CreateWithProgram(
      "group2-rol-byte-cl-8-test", "rol byte [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0008;  // CL = 8
  helper->memory_[0x0800] = 0x42;        // 01000010b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x42);  // 01000010b (same as original)
  helper->CheckFlags({{kCF, false}});
}

TEST_F(Group2Test, RolWordCL) {
  // Test case for ROL r/m16, CL (Opcode D3 /0)
  // Example: ROL word [bx], cl
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-rol-word-cl-test", "rol word [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;

  // Test 1: Rotate by 0 (no change, no flags affected)
  helper->cpu_.registers[kCX] = 0x0000;  // CL = 0
  helper->memory_[0x0800] = 0x55;        // Low byte
  helper->memory_[0x0801] = 0xAA;        // High byte (0xAA55)
  SetFlag(&helper->cpu_, kCF, true);     // Set carry to verify it's unchanged
  helper->ExecuteInstructions(1);
  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0xAA55);
  helper->CheckFlags({{kCF, true}});  // CF should remain unchanged

  // Test 2: Rotate by 4
  helper = CPUTestHelper::CreateWithProgram(
      "group2-rol-word-cl-4-test", "rol word [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0004;  // CL = 4
  helper->memory_[0x0800] = 0x34;        // Low byte
  helper->memory_[0x0801] = 0x12;        // High byte (0x1234)
  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x2341);  // 0x1234 rotated left by 4 = 0x2341
  helper->CheckFlags({{kCF, true}});

  // Test 3: Rotate by 8
  helper = CPUTestHelper::CreateWithProgram(
      "group2-rol-word-cl-8-test", "rol word [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0008;  // CL = 8
  helper->memory_[0x0800] = 0x34;        // Low byte
  helper->memory_[0x0801] = 0x12;        // High byte (0x1234)
  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x3412);  // Bytes swapped
  helper->CheckFlags({{kCF, false}});

  // Test 4: Rotate by 16 (full rotation, should be same as original)
  helper = CPUTestHelper::CreateWithProgram(
      "group2-rol-word-cl-16-test", "rol word [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0010;  // CL = 16
  helper->memory_[0x0800] = 0x34;        // Low byte
  helper->memory_[0x0801] = 0x12;        // High byte (0x1234)
  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x1234);  // Same as original
  helper->CheckFlags({{kCF, false}});
}

TEST_F(Group2Test, RolRegisterByte) {
  // Test case for ROL r8, 1 via ModR/M encoding
  // Example: ROL AL, 1
  auto helper =
      CPUTestHelper::CreateWithProgram("group2-rol-al-1-test", "rol al, 1\n");

  helper->cpu_.registers[kAX] = 0x1242;  // AL = 0x42
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x84);         // AL = 0x84
  EXPECT_EQ((helper->cpu_.registers[kAX] >> 8) & 0xFF, 0x12);  // AH unchanged
  helper->CheckFlags({{kCF, false}, {kOF, true}});

  // Test with BH register
  helper =
      CPUTestHelper::CreateWithProgram("group2-rol-bh-1-test", "rol bh, 1\n");
  helper->cpu_.registers[kBX] = 0x8078;  // BH = 0x80
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      (helper->cpu_.registers[kBX] >> 8) & 0xFF,
      0x01);  // BH = 0x01 (MSB rotated to LSB)
  EXPECT_EQ(helper->cpu_.registers[kBX] & 0xFF, 0x78);  // BL unchanged
  helper->CheckFlags({{kCF, true}, {kOF, true}});
}

TEST_F(Group2Test, RolRegisterWord) {
  // Test case for ROL r16, 1 via ModR/M encoding
  // Example: ROL AX, 1
  auto helper =
      CPUTestHelper::CreateWithProgram("group2-rol-ax-1-test", "rol ax, 1\n");

  helper->cpu_.registers[kAX] = 0x8234;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0469);  // MSB rotated to LSB
  helper->CheckFlags({{kCF, true}, {kOF, true}});

  // Test with CX register and CL count
  helper =
      CPUTestHelper::CreateWithProgram("group2-rol-cx-cl-test", "rol cx, cl\n");
  helper->cpu_.registers[kCX] = 0x1204;  // CH = 0x12, CL = 0x04
  // Note: CL is used as the rotate count, so CL = 0x04 = 4.
  // For the 8086, the lower 5 bits of CL are used, so the effective count is 4.
  helper->ExecuteInstructions(1);
  // 0x1204 rotated left by 4 = 0x2041
  EXPECT_EQ(helper->cpu_.registers[kCX], 0x2041);
  helper->CheckFlags({{kCF, true}});
}

TEST_F(Group2Test, RolMemoryWithDisplacement) {
  // Test case for ROL with memory operand using displacement
  // Example: ROL byte [bx+2], 1
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-rol-displacement-test", "rol byte [bx+2], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0802] = 0x81;  // 10000001b

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0802], 0x03);  // 00000011b (MSB rotated to LSB)
  helper->CheckFlags({{kCF, true}, {kOF, true}});
}

TEST_F(Group2Test, RolOverflowFlag) {
  // Test specific cases for overflow flag behavior
  // OF is set only for 1-bit rotates and when the sign bit changes

  // Test 1: No overflow (sign bit doesn't change)
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-rol-no-overflow-test", "rol al, 1\n");
  helper->cpu_.registers[kAX] = 0x0030;  // AL = 0x30 (00110000b)
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x60);  // AL = 0x60 (01100000b)
  helper->CheckFlags({{kOF, false}});  // No overflow, sign bit stayed 0

  // Test 2: Overflow (sign bit changes from 0 to 1)
  helper = CPUTestHelper::CreateWithProgram(
      "group2-rol-overflow-0to1-test", "rol al, 1\n");
  helper->cpu_.registers[kAX] = 0x0040;  // AL = 0x40 (01000000b)
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x80);  // AL = 0x80 (10000000b)
  helper->CheckFlags({{kOF, true}});  // Overflow, sign bit changed from 0 to 1

  // Test 3: Overflow (sign bit changes from 1 to 0)
  helper = CPUTestHelper::CreateWithProgram(
      "group2-rol-overflow-1to0-test", "rol al, 1\n");
  helper->cpu_.registers[kAX] = 0x0080;  // AL = 0x80 (10000000b)
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x01);  // AL = 0x01 (00000001b)
  helper->CheckFlags({{kCF, true}, {kOF, true}});
}

TEST_F(Group2Test, RorByte1) {
  // Test case for ROR r/m8, 1 (Opcode D0 /1)
  // Example: ROR byte [bx], 1
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-ror-byte-1-test", "ror byte [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;

  // Test 1: No carry, no overflow
  helper->memory_[0x0800] = 0x02;  // 00000010b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x01);  // 00000001b
  helper->CheckFlags({{kCF, false}, {kOF, false}});

  // Test 2: Carry generated (LSB rotates to MSB and CF)
  helper = CPUTestHelper::CreateWithProgram(
      "group2-ror-byte-1-carry-test", "ror byte [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x01;  // 00000001b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x80);  // 10000000b (LSB rotated to MSB)
  helper->CheckFlags({{kCF, true}, {kOF, true}});

  // Test 3: Multiple bits set
  helper = CPUTestHelper::CreateWithProgram(
      "group2-ror-byte-1-multiple-test", "ror byte [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x55;  // 01010101b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0xAA);  // 10101010b (LSB rotated to MSB)
  helper->CheckFlags({{kCF, true}, {kOF, true}});

  // Test 4: All bits set
  helper = CPUTestHelper::CreateWithProgram(
      "group2-ror-byte-1-all-bits-test", "ror byte [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xFF;  // 11111111b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0xFF);  // 11111111b (no change)
  helper->CheckFlags({{kCF, true}, {kOF, false}});
}

TEST_F(Group2Test, RorWord1) {
  // Test case for ROR r/m16, 1 (Opcode D1 /1)
  // Example: ROR word [bx], 1
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-ror-word-1-test", "ror word [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;

  // Test 1: No carry, no overflow
  helper->memory_[0x0800] = 0x00;  // Low byte
  helper->memory_[0x0801] = 0x20;  // High byte (0x2000)
  helper->ExecuteInstructions(1);
  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x1000);
  helper->CheckFlags({{kCF, false}, {kOF, false}});

  // Test 2: Carry generated (LSB rotates to MSB and CF)
  helper = CPUTestHelper::CreateWithProgram(
      "group2-ror-word-1-carry-test", "ror word [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x01;  // Low byte
  helper->memory_[0x0801] = 0x00;  // High byte (0x0001)
  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x8000);  // LSB rotated to MSB
  helper->CheckFlags({{kCF, true}, {kOF, true}});

  // Test 3: Multiple bits set
  helper = CPUTestHelper::CreateWithProgram(
      "group2-ror-word-1-multiple-test", "ror word [bx], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x55;  // Low byte
  helper->memory_[0x0801] = 0x55;  // High byte (0x5555)
  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0xAAAA);  // LSB rotated to MSB
  helper->CheckFlags({{kCF, true}, {kOF, true}});
}

TEST_F(Group2Test, RorByteCL) {
  // Test case for ROR r/m8, CL (Opcode D2 /1)
  // Example: ROR byte [bx], cl
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-ror-byte-cl-test", "ror byte [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;

  // Test 1: Rotate by 0 (no change, no flags affected)
  helper->cpu_.registers[kCX] = 0x0000;  // CL = 0
  helper->memory_[0x0800] = 0x55;
  SetFlag(&helper->cpu_, kCF, true);  // Set carry to verify it's unchanged
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x55);
  helper->CheckFlags({{kCF, true}});  // CF should remain unchanged

  // Test 2: Rotate by 2
  helper = CPUTestHelper::CreateWithProgram(
      "group2-ror-byte-cl-2-test", "ror byte [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0002;  // CL = 2
  helper->memory_[0x0800] = 0x15;        // 00010101b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x45);  // 01000101b (rotated right by 2)
  helper->CheckFlags(
      {{kCF, false}});  // CF is the last bit shifted out (bit 1 of 0x15 = 0)

  // Test 3: Rotate by 3
  helper = CPUTestHelper::CreateWithProgram(
      "group2-ror-byte-cl-3-test", "ror byte [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0003;  // CL = 3
  helper->memory_[0x0800] = 0x21;        // 00100001b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->memory_[0x0800],
      0x24);  // 00100100b (rotated right by 3: 001 + 00100)
  helper->CheckFlags(
      {{kCF, false}});  // CF is the last bit shifted out (bit 2 of 0x21 = 0)

  // Test 4: Rotate by 4
  helper = CPUTestHelper::CreateWithProgram(
      "group2-ror-byte-cl-4-test", "ror byte [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0004;  // CL = 4
  helper->memory_[0x0800] = 0xF0;        // 11110000b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x0F);  // 00001111b
  helper->CheckFlags({{kCF, false}});

  // Test 5: Rotate by 8 (full rotation, should be same as original)
  helper = CPUTestHelper::CreateWithProgram(
      "group2-ror-byte-cl-8-test", "ror byte [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0008;  // CL = 8
  helper->memory_[0x0800] = 0x42;        // 01000010b
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x42);  // 01000010b (same as original)
  helper->CheckFlags({{kCF, false}});
}

TEST_F(Group2Test, RorWordCL) {
  // Test case for ROR r/m16, CL (Opcode D3 /1)
  // Example: ROR word [bx], cl
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-ror-word-cl-test", "ror word [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;

  // Test 1: Rotate by 0 (no change, no flags affected)
  helper->cpu_.registers[kCX] = 0x0000;  // CL = 0
  helper->memory_[0x0800] = 0x55;        // Low byte
  helper->memory_[0x0801] = 0xAA;        // High byte (0xAA55)
  SetFlag(&helper->cpu_, kCF, true);     // Set carry to verify it's unchanged
  helper->ExecuteInstructions(1);
  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0xAA55);
  helper->CheckFlags({{kCF, true}});  // CF should remain unchanged

  // Test 2: Rotate by 4
  helper = CPUTestHelper::CreateWithProgram(
      "group2-ror-word-cl-4-test", "ror word [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0004;  // CL = 4
  helper->memory_[0x0800] = 0x34;        // Low byte
  helper->memory_[0x0801] = 0x12;        // High byte (0x1234)
  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x4123);  // 0x1234 rotated right by 4 = 0x4123
  helper->CheckFlags({{kCF, false}});

  // Test 3: Rotate by 8
  helper = CPUTestHelper::CreateWithProgram(
      "group2-ror-word-cl-8-test", "ror word [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0008;  // CL = 8
  helper->memory_[0x0800] = 0x34;        // Low byte
  helper->memory_[0x0801] = 0x12;        // High byte (0x1234)
  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x3412);  // Bytes swapped
  helper->CheckFlags({{kCF, false}});

  // Test 4: Rotate by 16 (full rotation, should be same as original)
  helper = CPUTestHelper::CreateWithProgram(
      "group2-ror-word-cl-16-test", "ror word [bx], cl\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kCX] = 0x0010;  // CL = 16
  helper->memory_[0x0800] = 0x34;        // Low byte
  helper->memory_[0x0801] = 0x12;        // High byte (0x1234)
  helper->ExecuteInstructions(1);
  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x1234);  // Same as original
  helper->CheckFlags({{kCF, false}});
}

TEST_F(Group2Test, RorRegisterByte) {
  // Test case for ROR r8, 1 via ModR/M encoding
  // Example: ROR AL, 1
  auto helper =
      CPUTestHelper::CreateWithProgram("group2-ror-al-1-test", "ror al, 1\n");

  helper->cpu_.registers[kAX] = 0x1242;  // AL = 0x42
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x21);         // AL = 0x21
  EXPECT_EQ((helper->cpu_.registers[kAX] >> 8) & 0xFF, 0x12);  // AH unchanged
  helper->CheckFlags({{kCF, false}, {kOF, false}});

  // Test with BH register
  helper =
      CPUTestHelper::CreateWithProgram("group2-ror-bh-1-test", "ror bh, 1\n");
  helper->cpu_.registers[kBX] = 0x8078;  // BH = 0x80
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      (helper->cpu_.registers[kBX] >> 8) & 0xFF,
      0x40);  // BH = 0x40 (rotated right by 1)
  EXPECT_EQ(helper->cpu_.registers[kBX] & 0xFF, 0x78);  // BL unchanged
  helper->CheckFlags({{kCF, false}, {kOF, true}});
}

TEST_F(Group2Test, RorRegisterWord) {
  // Test case for ROR r16, 1 via ModR/M encoding
  // Example: ROR AX, 1
  auto helper =
      CPUTestHelper::CreateWithProgram("group2-ror-ax-1-test", "ror ax, 1\n");

  helper->cpu_.registers[kAX] = 0x8234;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x411A);  // LSB rotated to MSB
  helper->CheckFlags({{kCF, false}, {kOF, true}});

  // Test with CX register and CL count
  helper =
      CPUTestHelper::CreateWithProgram("group2-ror-cx-cl-test", "ror cx, cl\n");
  helper->cpu_.registers[kCX] = 0x1204;  // CH = 0x12, CL = 0x04
  // Note: CL is used as the rotate count, so CL = 0x04 = 4.
  // For the 8086, the lower 5 bits of CL are used, so the effective count is 4.
  helper->ExecuteInstructions(1);
  // 0x1204 rotated right by 4 = 0x4120
  EXPECT_EQ(helper->cpu_.registers[kCX], 0x4120);
  helper->CheckFlags({{kCF, false}});
}

TEST_F(Group2Test, RorMemoryWithDisplacement) {
  // Test case for ROR with memory operand using displacement
  // Example: ROR byte [bx+2], 1
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-ror-displacement-test", "ror byte [bx+2], 1\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0802] = 0x81;  // 10000001b

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0802], 0xC0);  // 11000000b (LSB rotated to MSB)
  helper->CheckFlags(
      {{kCF, true},
       {kOF, false}});  // CF=1 (LSB of 0x81), OF=0 (MSB unchanged: 1->1)
}

TEST_F(Group2Test, RorOverflowFlag) {
  // Test specific cases for overflow flag behavior
  // OF is set only for 1-bit rotates and when the sign bit changes

  // Test 1: No overflow (sign bit doesn't change)
  auto helper = CPUTestHelper::CreateWithProgram(
      "group2-ror-no-overflow-test", "ror al, 1\n");
  helper->cpu_.registers[kAX] = 0x0030;  // AL = 0x30 (00110000b)
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x18);  // AL = 0x18 (00011000b)
  helper->CheckFlags({{kOF, false}});  // No overflow, sign bit stayed 0

  // Test 2: Overflow (sign bit changes from 0 to 1)
  helper = CPUTestHelper::CreateWithProgram(
      "group2-ror-overflow-0to1-test", "ror al, 1\n");
  helper->cpu_.registers[kAX] = 0x0001;  // AL = 0x01 (00000001b)
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x80);  // AL = 0x80 (10000000b)
  helper->CheckFlags({{kOF, true}});  // Overflow, sign bit changed from 0 to 1

  // Test 3: Overflow (sign bit changes from 1 to 0)
  helper = CPUTestHelper::CreateWithProgram(
      "group2-ror-overflow-1to0-test", "ror al, 1\n");
  helper->cpu_.registers[kAX] = 0x0080;  // AL = 0x80 (10000000b)
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x40);  // AL = 0x40 (01000000b)
  helper->CheckFlags({{kCF, false}, {kOF, true}});
}