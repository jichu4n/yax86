#include <gtest/gtest.h>

#include "../cpu.h"
#include "./test_helpers.h"

using namespace std;

class Group3Test : public ::testing::Test {};

TEST_F(Group3Test, TestImmediateByte) {
  // Test case for TEST r/m8, imm8 (Opcode F6 /0 ib)
  // Example: TEST byte [bx], 0x0F
  auto helper = CPUTestHelper::CreateWithProgram(
      "group3-test-rm8-imm8-test", "test byte [bx], 0x0F\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;  // Point BX to some memory location
  helper->memory_[0x0800] = 0xF0;        // 11110000b

  helper->ExecuteInstructions(1);

  // TEST performs bitwise AND but doesn't store result, only sets flags
  EXPECT_EQ(helper->memory_[0x0800], 0xF0);  // Memory unchanged
  // 11110000b & 00001111b = 00000000b (zero result)
  helper->CheckFlags(
      {{kZF, true}, {kSF, false}, {kPF, true}, {kCF, false}, {kOF, false}});

  // Test with non-zero result
  helper = CPUTestHelper::CreateWithProgram(
      "group3-test-rm8-imm8-nonzero-test", "test byte [bx], 0xFF\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xAA;  // 10101010b

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->memory_[0x0800], 0xAA);  // Memory unchanged
  // 10101010b & 11111111b = 10101010b (non-zero, negative result, 4 bits = even
  // parity)
  helper->CheckFlags(
      {{kZF, false}, {kSF, true}, {kPF, true}, {kCF, false}, {kOF, false}});

  // Test with odd parity
  helper = CPUTestHelper::CreateWithProgram(
      "group3-test-rm8-imm8-odd-parity-test", "test byte [bx], 0x07\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x07;  // 00000111b

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->memory_[0x0800], 0x07);  // Memory unchanged
  // 00000111b & 00000111b = 00000111b (3 bits set = odd parity)
  helper->CheckFlags(
      {{kZF, false}, {kSF, false}, {kPF, false}, {kCF, false}, {kOF, false}});
}

TEST_F(Group3Test, TestImmediateWord) {
  // Test case for TEST r/m16, imm16 (Opcode F7 /0 iw)
  // Example: TEST word [bx], 0x00FF
  auto helper = CPUTestHelper::CreateWithProgram(
      "group3-test-rm16-imm16-test", "test word [bx], 0x00FF\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x00;  // Low byte
  helper->memory_[0x0801] = 0xFF;  // High byte (0xFF00)

  helper->ExecuteInstructions(1);

  // Memory should be unchanged
  uint16_t memory_value =
      helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(memory_value, 0xFF00);
  // 0xFF00 & 0x00FF = 0x0000 (zero result)
  helper->CheckFlags(
      {{kZF, true}, {kSF, false}, {kPF, true}, {kCF, false}, {kOF, false}});

  // Test with non-zero result
  helper = CPUTestHelper::CreateWithProgram(
      "group3-test-rm16-imm16-nonzero-test", "test word [bx], 0xFFFF\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x34;  // Low byte
  helper->memory_[0x0801] = 0x12;  // High byte (0x1234)

  helper->ExecuteInstructions(1);

  memory_value = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(memory_value, 0x1234);  // Memory unchanged
  // 0x1234 & 0xFFFF = 0x1234 (non-zero, positive result, low byte 0x34 has 3
  // bits = odd parity)
  helper->CheckFlags(
      {{kZF, false}, {kSF, false}, {kPF, false}, {kCF, false}, {kOF, false}});

  // Test with negative result (sign bit set)
  helper = CPUTestHelper::CreateWithProgram(
      "group3-test-rm16-imm16-negative-test", "test word [bx], 0x8000\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x00;  // Low byte
  helper->memory_[0x0801] = 0x80;  // High byte (0x8000)

  helper->ExecuteInstructions(1);

  memory_value = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(memory_value, 0x8000);  // Memory unchanged
  // 0x8000 & 0x8000 = 0x8000 (non-zero, negative result, low byte 0x00 has 0
  // bits = even parity)
  helper->CheckFlags(
      {{kZF, false}, {kSF, true}, {kPF, true}, {kCF, false}, {kOF, false}});
}

TEST_F(Group3Test, TestRegisterByte) {
  // Test case for TEST r8, imm8 via ModR/M encoding
  // Example: TEST AL, 0x55
  auto helper = CPUTestHelper::CreateWithProgram(
      "group3-test-al-imm8-test", "test al, 0x55\n");

  // Test 1: Zero result
  helper->cpu_.registers[kAX] = 0xAA00;  // AL = 0xAA (10101010b)
  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0xAA00);  // Register unchanged
  // 10101010b & 01010101b = 00000000b (zero result)
  helper->CheckFlags(
      {{kZF, true}, {kSF, false}, {kPF, true}, {kCF, false}, {kOF, false}});

  // Test 2: Non-zero result
  helper = CPUTestHelper::CreateWithProgram(
      "group3-test-al-imm8-nonzero-test", "test al, 0xFF\n");
  helper->cpu_.registers[kAX] = 0x0042;  // AL = 0x42 (01000010b)
  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0042);  // Register unchanged
  // 01000010b & 11111111b = 01000010b (non-zero, positive result, 2 bits = even
  // parity)
  helper->CheckFlags(
      {{kZF, false}, {kSF, false}, {kPF, true}, {kCF, false}, {kOF, false}});
}

TEST_F(Group3Test, TestRegisterWord) {
  // Test case for TEST r16, imm16 via ModR/M encoding
  // Example: TEST AX, 0x5555
  auto helper = CPUTestHelper::CreateWithProgram(
      "group3-test-ax-imm16-test", "test ax, 0x5555\n");

  // Test 1: Zero result
  helper->cpu_.registers[kAX] = 0xAAAA;  // 1010101010101010b
  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0xAAAA);  // Register unchanged
  // 1010101010101010b & 0101010101010101b = 0000000000000000b (zero result)
  helper->CheckFlags(
      {{kZF, true}, {kSF, false}, {kPF, true}, {kCF, false}, {kOF, false}});

  // Test 2: Non-zero result with even parity
  helper = CPUTestHelper::CreateWithProgram(
      "group3-test-ax-imm16-even-parity-test", "test ax, 0x0003\n");
  helper->cpu_.registers[kAX] = 0x0003;  // 0000000000000011b
  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0003);  // Register unchanged
  // 0000000000000011b & 0000000000000011b = 0000000000000011b (2 bits set =
  // even parity)
  helper->CheckFlags(
      {{kZF, false}, {kSF, false}, {kPF, true}, {kCF, false}, {kOF, false}});
}

TEST_F(Group3Test, TestMemoryWithDisplacement) {
  // Test case for TEST with memory operand using displacement
  // Example: TEST byte [bx+2], 0x80
  auto helper = CPUTestHelper::CreateWithProgram(
      "group3-test-displacement-test", "test byte [bx+2], 0x80\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0802] = 0x80;  // Memory at BX+2

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->memory_[0x0802], 0x80);  // Memory unchanged
  // 10000000b & 10000000b = 10000000b (negative result, single bit set = odd
  // parity)
  helper->CheckFlags(
      {{kZF, false}, {kSF, true}, {kPF, false}, {kCF, false}, {kOF, false}});
}

TEST_F(Group3Test, NotByte) {
  // Test case for NOT r/m8 (Opcode F6 /2)
  // Example: NOT byte [bx]
  auto helper = CPUTestHelper::CreateWithProgram(
      "group3-not-rm8-test", "not byte [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xAA;  // 10101010b

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->memory_[0x0800], 0x55);  // ~10101010b = 01010101b
  // NOT instruction does not affect any flags

  // Test with zero value
  helper = CPUTestHelper::CreateWithProgram(
      "group3-not-rm8-zero-test", "not byte [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x00;  // 00000000b

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->memory_[0x0800], 0xFF);  // ~00000000b = 11111111b

  // Test with all bits set
  helper = CPUTestHelper::CreateWithProgram(
      "group3-not-rm8-allbits-test", "not byte [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xFF;  // 11111111b

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->memory_[0x0800], 0x00);  // ~11111111b = 00000000b
}

TEST_F(Group3Test, NotWord) {
  // Test case for NOT r/m16 (Opcode F7 /2)
  // Example: NOT word [bx]
  auto helper = CPUTestHelper::CreateWithProgram(
      "group3-not-rm16-test", "not word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xAA;  // Low byte
  helper->memory_[0x0801] = 0x55;  // High byte (0x55AA)

  helper->ExecuteInstructions(1);

  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0xAA55);  // ~0x55AA = 0xAA55

  // Test with zero value
  helper = CPUTestHelper::CreateWithProgram(
      "group3-not-rm16-zero-test", "not word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x00;  // Low byte
  helper->memory_[0x0801] = 0x00;  // High byte (0x0000)

  helper->ExecuteInstructions(1);

  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0xFFFF);  // ~0x0000 = 0xFFFF

  // Test with all bits set
  helper = CPUTestHelper::CreateWithProgram(
      "group3-not-rm16-allbits-test", "not word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xFF;  // Low byte
  helper->memory_[0x0801] = 0xFF;  // High byte (0xFFFF)

  helper->ExecuteInstructions(1);

  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0000);  // ~0xFFFF = 0x0000
}

TEST_F(Group3Test, NotRegisterByte) {
  // Test case for NOT r8 via ModR/M encoding
  // Example: NOT AL
  auto helper =
      CPUTestHelper::CreateWithProgram("group3-not-al-test", "not al\n");

  helper->cpu_.registers[kAX] = 0x1234;  // AL = 0x34
  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x12CB);  // AL becomes ~0x34 = 0xCB

  // Test with BH register
  helper = CPUTestHelper::CreateWithProgram("group3-not-bh-test", "not bh\n");
  helper->cpu_.registers[kBX] = 0x5678;  // BH = 0x56
  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kBX], 0xA978);  // BH becomes ~0x56 = 0xA9
}

TEST_F(Group3Test, NotRegisterWord) {
  // Test case for NOT r16 via ModR/M encoding
  // Example: NOT AX
  auto helper =
      CPUTestHelper::CreateWithProgram("group3-not-ax-test", "not ax\n");

  helper->cpu_.registers[kAX] = 0x1234;
  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0xEDCB);  // ~0x1234 = 0xEDCB

  // Test with CX register
  helper = CPUTestHelper::CreateWithProgram("group3-not-cx-test", "not cx\n");
  helper->cpu_.registers[kCX] = 0xAAAA;
  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kCX], 0x5555);  // ~0xAAAA = 0x5555
}

TEST_F(Group3Test, NegByte) {
  // Test case for NEG r/m8 (Opcode F6 /3)
  // Example: NEG byte [bx]
  auto helper = CPUTestHelper::CreateWithProgram(
      "group3-neg-rm8-test", "neg byte [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x01;  // Positive value

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->memory_[0x0800], 0xFF);  // -1 in two's complement
  // NEG sets flags like SUB 0, operand. Result 0xFF has 8 bits set = even
  // parity
  helper->CheckFlags(
      {{kZF, false}, {kSF, true}, {kPF, true}, {kCF, true}, {kOF, false}});

  // Test with zero value (special case)
  helper = CPUTestHelper::CreateWithProgram(
      "group3-neg-rm8-zero-test", "neg byte [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x00;

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->memory_[0x0800], 0x00);  // -0 = 0
  helper->CheckFlags(
      {{kZF, true}, {kSF, false}, {kPF, true}, {kCF, false}, {kOF, false}});

  // Test with maximum positive value (overflow case)
  helper = CPUTestHelper::CreateWithProgram(
      "group3-neg-rm8-overflow-test", "neg byte [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x80;  // -128 in signed 8-bit

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->memory_[0x0800], 0x80);  // -(-128) = -128 (overflow)
  // Result 0x80 has 1 bit set = odd parity
  helper->CheckFlags(
      {{kZF, false}, {kSF, true}, {kPF, false}, {kCF, true}, {kOF, true}});

  // Test with negative value
  helper = CPUTestHelper::CreateWithProgram(
      "group3-neg-rm8-negative-test", "neg byte [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xFE;  // -2 in two's complement

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->memory_[0x0800], 0x02);  // -(-2) = 2
  helper->CheckFlags(
      {{kZF, false}, {kSF, false}, {kPF, false}, {kCF, true}, {kOF, false}});
}

TEST_F(Group3Test, NegWord) {
  // Test case for NEG r/m16 (Opcode F7 /3)
  // Example: NEG word [bx]
  auto helper = CPUTestHelper::CreateWithProgram(
      "group3-neg-rm16-test", "neg word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x01;  // Low byte
  helper->memory_[0x0801] = 0x00;  // High byte (0x0001)

  helper->ExecuteInstructions(1);

  uint16_t result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0xFFFF);  // -1 in two's complement
  helper->CheckFlags(
      {{kZF, false}, {kSF, true}, {kPF, true}, {kCF, true}, {kOF, false}});

  // Test with zero value
  helper = CPUTestHelper::CreateWithProgram(
      "group3-neg-rm16-zero-test", "neg word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x00;  // Low byte
  helper->memory_[0x0801] = 0x00;  // High byte (0x0000)

  helper->ExecuteInstructions(1);

  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0000);  // -0 = 0
  helper->CheckFlags(
      {{kZF, true}, {kSF, false}, {kPF, true}, {kCF, false}, {kOF, false}});

  // Test with maximum positive value (overflow case)
  helper = CPUTestHelper::CreateWithProgram(
      "group3-neg-rm16-overflow-test", "neg word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x00;  // Low byte
  helper->memory_[0x0801] = 0x80;  // High byte (0x8000 = -32768)

  helper->ExecuteInstructions(1);

  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x8000);  // -(-32768) = -32768 (overflow)
  helper->CheckFlags(
      {{kZF, false}, {kSF, true}, {kPF, true}, {kCF, true}, {kOF, true}});

  // Test with negative value
  helper = CPUTestHelper::CreateWithProgram(
      "group3-neg-rm16-negative-test", "neg word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xFE;  // Low byte
  helper->memory_[0x0801] = 0xFF;  // High byte (0xFFFE = -2)

  helper->ExecuteInstructions(1);

  result = helper->memory_[0x0800] | (helper->memory_[0x0801] << 8);
  EXPECT_EQ(result, 0x0002);  // -(-2) = 2
  helper->CheckFlags(
      {{kZF, false}, {kSF, false}, {kPF, false}, {kCF, true}, {kOF, false}});
}

TEST_F(Group3Test, NegRegisterByte) {
  // Test case for NEG r8 via ModR/M encoding
  // Example: NEG AL
  auto helper =
      CPUTestHelper::CreateWithProgram("group3-neg-al-test", "neg al\n");

  helper->cpu_.registers[kAX] = 0x1205;  // AL = 0x05
  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x12FB);  // AL becomes -5 = 0xFB

  // Test with BH register
  helper = CPUTestHelper::CreateWithProgram("group3-neg-bh-test", "neg bh\n");
  helper->cpu_.registers[kBX] = 0x0A78;  // BH = 0x0A
  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kBX], 0xF678);  // BH becomes -10 = 0xF6
}

TEST_F(Group3Test, NegRegisterWord) {
  // Test case for NEG r16 via ModR/M encoding
  // Example: NEG AX
  auto helper =
      CPUTestHelper::CreateWithProgram("group3-neg-ax-test", "neg ax\n");

  helper->cpu_.registers[kAX] = 0x1234;
  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0xEDCC);  // ~0x1234 = 0xEDCC

  // Test with CX register
  helper = CPUTestHelper::CreateWithProgram("group3-neg-cx-test", "neg cx\n");
  helper->cpu_.registers[kCX] = 0xAAAA;
  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kCX], 0x5556);  // ~0xAAAA = 0x5556
}

TEST_F(Group3Test, MulByte) {
  // Test case for MUL r/m8 (Opcode F6 /4)
  // Example: MUL byte [bx] (AX = AL * byte [bx])

  // Case 1: No overflow
  auto helper = CPUTestHelper::CreateWithProgram(
      "group3-mul-byte-no-overflow", "mul byte [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0x0002;  // AL = 0x02
  helper->memory_[0x0800] = 0x03;        // [bx] = 0x03
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0006);  // AX = 0x02 * 0x03 = 0x0006
  helper->CheckFlags({{kCF, false}, {kOF, false}});

  // Case 2: Overflow into AH
  helper = CPUTestHelper::CreateWithProgram(
      "group3-mul-byte-overflow", "mul byte [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0x00FF;  // AL = 0xFF (255)
  helper->memory_[0x0800] = 0x02;        // [bx] = 0x02
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x01FE);  // AX = 255 * 2 = 510 = 0x01FE
  helper->CheckFlags({{kCF, true}, {kOF, true}});

  // Case 3: Another overflow example
  helper = CPUTestHelper::CreateWithProgram(
      "group3-mul-byte-overflow-2", "mul byte [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0x0080;  // AL = 0x80 (128)
  helper->memory_[0x0800] = 0x02;        // [bx] = 0x02
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x0100);  // AX = 128 * 2 = 256 = 0x0100
  helper->CheckFlags({{kCF, true}, {kOF, true}});
}

TEST_F(Group3Test, MulWord) {
  // Test case for MUL r/m16 (Opcode F7 /4)
  // Example: MUL word [bx] (DX:AX = AX * word [bx])

  // Case 1: No overflow
  auto helper = CPUTestHelper::CreateWithProgram(
      "group3-mul-word-no-overflow", "mul word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0x0002;  // AX = 2
  helper->memory_[0x0800] = 0x03;        // [bx] = 3 (low byte)
  helper->memory_[0x0801] = 0x00;        // (high byte)
  helper->cpu_.registers[kDX] =
      0x5555;  // Pre-set DX to check it's correctly overwritten
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0006);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x0000);
  helper->CheckFlags({{kCF, false}, {kOF, false}});

  // Case 2: Overflow into DX
  helper = CPUTestHelper::CreateWithProgram(
      "group3-mul-word-overflow", "mul word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0xFFFF;  // AX = 65535
  helper->memory_[0x0800] = 0x02;        // [bx] = 2
  helper->memory_[0x0801] = 0x00;
  helper->cpu_.registers[kDX] = 0x5555;
  helper->ExecuteInstructions(1);
  // 65535 * 2 = 131070 = 0x0001FFFE
  EXPECT_EQ(helper->cpu_.registers[kAX], 0xFFFE);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x0001);
  helper->CheckFlags({{kCF, true}, {kOF, true}});

  // Case 3: Max AX * Max [bx] (word)
  helper = CPUTestHelper::CreateWithProgram(
      "group3-mul-word-overflow-max", "mul word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0xFFFF;  // AX = 65535
  helper->memory_[0x0800] = 0xFF;        // [bx] = 65535 (low byte)
  helper->memory_[0x0801] = 0xFF;        // (high byte)
  helper->cpu_.registers[kDX] = 0x5555;
  helper->ExecuteInstructions(1);
  // 65535 * 65535 = 4294836225 = 0xFFFE0001
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0001);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0xFFFE);
  helper->CheckFlags({{kCF, true}, {kOF, true}});

  // Case 4: 0x8000 * 0xFFFF (unsigned interpretation for MUL)
  helper = CPUTestHelper::CreateWithProgram(
      "group3-mul-word-specific-overflow", "mul word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0x8000;  // AX = 32768
  helper->memory_[0x0800] = 0xFF;        // [bx] = 65535 (low byte)
  helper->memory_[0x0801] = 0xFF;        // (high byte)
  helper->cpu_.registers[kDX] = 0x5555;
  helper->ExecuteInstructions(1);
  // 32768 * 65535 = 2147450880 = 0x7FFF8000
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x8000);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x7FFF);
  helper->CheckFlags({{kCF, true}, {kOF, true}});
}

TEST_F(Group3Test, MulRegisterByte) {
  // Test case for MUL r8 (Opcode F6 /4, ModR/M specifies register)
  // Example: MUL CL (AX = AL * CL)

  // Case 1: No overflow
  auto helper =
      CPUTestHelper::CreateWithProgram("group3-mul-cl-no-overflow", "mul cl\n");
  helper->cpu_.registers[kAX] = 0x0002;  // AL = 0x02
  helper->cpu_.registers[kCX] = 0x0003;  // CL = 0x03
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0006);  // AX = 0x02 * 0x03 = 0x0006
  helper->CheckFlags({{kCF, false}, {kOF, false}});

  // Case 2: Overflow into AH
  helper =
      CPUTestHelper::CreateWithProgram("group3-mul-cl-overflow", "mul cl\n");
  helper->cpu_.registers[kAX] = 0x00FF;  // AL = 0xFF (255)
  helper->cpu_.registers[kCX] = 0x0002;  // CL = 0x02
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x01FE);  // AX = 255 * 2 = 510 = 0x01FE
  helper->CheckFlags({{kCF, true}, {kOF, true}});

  // Case 3: Another overflow example with BH
  helper =
      CPUTestHelper::CreateWithProgram("group3-mul-bh-overflow", "mul bh\n");
  helper->cpu_.registers[kAX] = 0x0080;  // AL = 0x80 (128)
  helper->cpu_.registers[kBX] = 0x0200;  // BH = 0x02
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x0100);  // AX = 128 * 2 = 256 = 0x0100
  helper->CheckFlags({{kCF, true}, {kOF, true}});

  // Case 4: Max AL * Max DL
  helper = CPUTestHelper::CreateWithProgram("group3-mul-dl-max", "mul dl\n");
  helper->cpu_.registers[kAX] = 0x00FF;  // AL = 0xFF
  helper->cpu_.registers[kDX] = 0x00FF;  // DL = 0xFF
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0xFE01);  // AX = 255 * 255 = 65025 = 0xFE01
  helper->CheckFlags({{kCF, true}, {kOF, true}});
}

TEST_F(Group3Test, MulRegisterWord) {
  // Test case for MUL r16 (Opcode F7 /4, ModR/M specifies register)
  // Example: MUL CX (DX:AX = AX * CX)

  // Case 1: No overflow
  auto helper =
      CPUTestHelper::CreateWithProgram("group3-mul-cx-no-overflow", "mul cx\n");
  helper->cpu_.registers[kAX] = 0x0002;  // AX = 2
  helper->cpu_.registers[kCX] = 0x0003;  // CX = 3
  helper->cpu_.registers[kDX] = 0x5555;  // Pre-set DX
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0006);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x0000);
  helper->CheckFlags({{kCF, false}, {kOF, false}});

  // Case 2: Overflow into DX
  helper =
      CPUTestHelper::CreateWithProgram("group3-mul-cx-overflow", "mul cx\n");
  helper->cpu_.registers[kAX] = 0xFFFF;  // AX = 65535
  helper->cpu_.registers[kCX] = 0x0002;  // CX = 2
  helper->cpu_.registers[kDX] = 0x5555;  // Pre-set DX
  helper->ExecuteInstructions(1);
  // 65535 * 2 = 131070 = 0x0001FFFE
  EXPECT_EQ(helper->cpu_.registers[kAX], 0xFFFE);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x0001);
  helper->CheckFlags({{kCF, true}, {kOF, true}});

  // Case 3: Max AX * Max CX
  helper = CPUTestHelper::CreateWithProgram("group3-mul-cx-max", "mul cx\n");
  helper->cpu_.registers[kAX] = 0xFFFF;  // AX = 65535
  helper->cpu_.registers[kCX] = 0xFFFF;  // CX = 65535
  helper->cpu_.registers[kDX] = 0x5555;  // Pre-set DX
  helper->ExecuteInstructions(1);
  // 65535 * 65535 = 4294836225 = 0xFFFE0001
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0001);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0xFFFE);
  helper->CheckFlags({{kCF, true}, {kOF, true}});

  // Case 4: 0x8000 * 0xFFFF (unsigned interpretation for MUL)
  helper = CPUTestHelper::CreateWithProgram(
      "group3-mul-cx-specific-overflow", "mul cx\n");
  helper->cpu_.registers[kAX] = 0x8000;  // AX = 32768
  helper->cpu_.registers[kCX] = 0xFFFF;  // CX = 65535
  helper->cpu_.registers[kDX] = 0x5555;  // Pre-set DX
  helper->ExecuteInstructions(1);
  // 32768 * 65535 = 2147450880 = 0x7FFF8000
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x8000);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x7FFF);
  helper->CheckFlags({{kCF, true}, {kOF, true}});
}

TEST_F(Group3Test, DivByte) {
  // Test case for DIV r/m8 (Opcode F6 /6)
  // Example: DIV byte [bx] (AL = AX / byte [bx], AH = AX % byte [bx])

  // Case 1: Normal division, no remainder
  auto helper = CPUTestHelper::CreateWithProgram(
      "group3-div-byte-normal-no-remainder", "div byte [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0x0014;  // AX = 20
  helper->memory_[0x0800] = 0x04;        // [bx] = 4
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x0005);  // AL = 20/4 = 5, AH = 20%4 = 0

  // Case 2: Normal division with remainder
  helper = CPUTestHelper::CreateWithProgram(
      "group3-div-byte-normal-with-remainder", "div byte [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0x0017;  // AX = 23
  helper->memory_[0x0800] = 0x05;        // [bx] = 5
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x0304);  // AL = 23/5 = 4, AH = 23%5 = 3

  // Case 3: Large dividend using AH:AL
  helper = CPUTestHelper::CreateWithProgram(
      "group3-div-byte-large-dividend", "div byte [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] =
      0x0102;  // AH = 1, AL = 2, so dividend = 256 + 2 = 258
  helper->memory_[0x0800] = 0x03;  // [bx] = 3
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x0056);  // AL = 258/3 = 86, AH = 258%3 = 0

  // Case 4: Division by 1
  helper = CPUTestHelper::CreateWithProgram(
      "group3-div-byte-by-one", "div byte [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0x00FF;  // AX = 255
  helper->memory_[0x0800] = 0x01;        // [bx] = 1
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x00FF);  // AL = 255/1 = 255, AH = 255%1 = 0

  // Case 5: Test with register operand
  helper =
      CPUTestHelper::CreateWithProgram("group3-div-byte-register", "div cl\n");
  helper->cpu_.registers[kAX] = 0x0030;  // AX = 48
  helper->cpu_.registers[kCX] = 0x0006;  // CL = 6
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x0008);  // AL = 48/6 = 8, AH = 48%6 = 0
}

TEST_F(Group3Test, DivWord) {
  // Test case for DIV r/m16 (Opcode F7 /6)
  // Example: DIV word [bx] (AX = DX:AX / word [bx], DX = DX:AX % word [bx])

  // Case 1: Normal division, no remainder
  auto helper = CPUTestHelper::CreateWithProgram(
      "group3-div-word-normal-no-remainder", "div word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0x0014;  // AX = 20
  helper->cpu_.registers[kDX] = 0x0000;  // DX = 0, so dividend = 20
  helper->memory_[0x0800] = 0x04;        // [bx] = 4 (low byte)
  helper->memory_[0x0801] = 0x00;        // (high byte)
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0005);  // AX = 20/4 = 5
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x0000);  // DX = 20%4 = 0

  // Case 2: Normal division with remainder
  helper = CPUTestHelper::CreateWithProgram(
      "group3-div-word-normal-with-remainder", "div word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0x0017;  // AX = 23
  helper->cpu_.registers[kDX] = 0x0000;  // DX = 0, so dividend = 23
  helper->memory_[0x0800] = 0x05;        // [bx] = 5
  helper->memory_[0x0801] = 0x00;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0004);  // AX = 23/5 = 4
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x0003);  // DX = 23%5 = 3

  // Case 3: Large dividend using DX:AX
  helper = CPUTestHelper::CreateWithProgram(
      "group3-div-word-large-dividend", "div word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0x0002;  // AX = 2
  helper->cpu_.registers[kDX] =
      0x0001;                      // DX = 1, so dividend = 65536 + 2 = 65538
  helper->memory_[0x0800] = 0x03;  // [bx] = 3
  helper->memory_[0x0801] = 0x00;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x5556);  // AX = 65538/3 = 21846
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x0000);  // DX = 65538%3 = 0

  // Case 4: Division by 1
  helper = CPUTestHelper::CreateWithProgram(
      "group3-div-word-by-one", "div word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0xFFFF;  // AX = 65535
  helper->cpu_.registers[kDX] = 0x0000;  // DX = 0
  helper->memory_[0x0800] = 0x01;        // [bx] = 1
  helper->memory_[0x0801] = 0x00;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0xFFFF);  // AX = 65535/1 = 65535
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x0000);  // DX = 65535%1 = 0

  // Case 5: Test with register operand
  helper =
      CPUTestHelper::CreateWithProgram("group3-div-word-register", "div cx\n");
  helper->cpu_.registers[kAX] = 0x0030;  // AX = 48
  helper->cpu_.registers[kDX] = 0x0000;  // DX = 0
  helper->cpu_.registers[kCX] = 0x0006;  // CX = 6
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0008);  // AX = 48/6 = 8
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x0000);  // DX = 48%6 = 0
}

TEST_F(Group3Test, IdivByte) {
  // Test case for IDIV r/m8 (Opcode F6 /7)
  // Example: IDIV byte [bx] (AL = AX / byte [bx], AH = AX % byte [bx])
  // All operations are signed

  // Case 1: Positive dividend, positive divisor, no remainder
  auto helper = CPUTestHelper::CreateWithProgram(
      "group3-idiv-byte-pos-pos-no-remainder", "idiv byte [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0x0014;  // AX = 20
  helper->memory_[0x0800] = 0x04;        // [bx] = 4
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x0005);  // AL = 20/4 = 5, AH = 20%4 = 0

  // Case 2: Positive dividend, positive divisor, with remainder
  helper = CPUTestHelper::CreateWithProgram(
      "group3-idiv-byte-pos-pos-with-remainder", "idiv byte [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0x0017;  // AX = 23
  helper->memory_[0x0800] = 0x05;        // [bx] = 5
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x0304);  // AL = 23/5 = 4, AH = 23%5 = 3

  // Case 3: Positive dividend, negative divisor
  helper = CPUTestHelper::CreateWithProgram(
      "group3-idiv-byte-pos-neg", "idiv byte [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0x0014;  // AX = 20
  helper->memory_[0x0800] = 0xFC;        // [bx] = -4
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX],
      0x00FB);  // AL = 20/(-4) = -5, AH = 20%(-4) = 0

  // Case 4: Negative dividend, positive divisor
  helper = CPUTestHelper::CreateWithProgram(
      "group3-idiv-byte-neg-pos", "idiv byte [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0xFFEC;  // AH = 0xFF, AL = 0xEC, so dividend =
                                         // 0xFFEC = -20 (signed 16-bit)
  helper->memory_[0x0800] = 0x04;        // [bx] = 4
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX],
      0x00FB);  // AL = -20/4 = -5 (0xFB), AH = -20%4 = 0

  // Case 5: Negative dividend, negative divisor
  helper = CPUTestHelper::CreateWithProgram(
      "group3-idiv-byte-neg-neg", "idiv byte [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0xFFEC;  // AH = 0xFF, AL = 0xEC, so dividend =
                                         // 0xFFEC = -20 (signed 16-bit)
  helper->memory_[0x0800] = 0xFC;        // [bx] = -4
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX],
      0x0005);  // AL = -20/(-4) = 5, AH = -20%(-4) = 0

  // Case 6: Division with negative remainder
  helper = CPUTestHelper::CreateWithProgram(
      "group3-idiv-byte-neg-remainder", "idiv byte [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0xFFE9;  // AH = 0xFF, AL = 0xE9, so dividend =
                                         // 0xFFE9 = -23 (signed 16-bit)
  helper->memory_[0x0800] = 0x05;        // [bx] = 5
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX],
      0xFDFC);  // AL = -23/5 = -4, AH = -23%5 = -3

  // Case 7: Test with register operand
  helper = CPUTestHelper::CreateWithProgram(
      "group3-idiv-byte-register", "idiv cl\n");
  helper->cpu_.registers[kAX] = 0x0030;  // AX = 48
  helper->cpu_.registers[kCX] = 0x00F4;  // CL = -12
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX],
      0x00FC);  // AL = 48/(-12) = -4, AH = 48%(-12) = 0
}

TEST_F(Group3Test, IdivWord) {
  // Test case for IDIV r/m16 (Opcode F7 /7)
  // Example: IDIV word [bx] (AX = DX:AX / word [bx], DX = DX:AX % word [bx])
  // All operations are signed

  // Case 1: Positive dividend, positive divisor, no remainder
  auto helper = CPUTestHelper::CreateWithProgram(
      "group3-idiv-word-pos-pos-no-remainder", "idiv word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0x0014;  // AX = 20
  helper->cpu_.registers[kDX] = 0x0000;  // DX = 0, so dividend = 20
  helper->memory_[0x0800] = 0x04;        // [bx] = 4
  helper->memory_[0x0801] = 0x00;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0005);  // AX = 20/4 = 5
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x0000);  // DX = 20%4 = 0

  // Case 2: Positive dividend, positive divisor, with remainder
  helper = CPUTestHelper::CreateWithProgram(
      "group3-idiv-word-pos-pos-with-remainder", "idiv word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0x0017;  // AX = 23
  helper->cpu_.registers[kDX] = 0x0000;  // DX = 0, so dividend = 23
  helper->memory_[0x0800] = 0x05;        // [bx] = 5
  helper->memory_[0x0801] = 0x00;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0004);  // AX = 23/5 = 4
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x0003);  // DX = 23%5 = 3

  // Case 3: Positive dividend, negative divisor
  helper = CPUTestHelper::CreateWithProgram(
      "group3-idiv-word-pos-neg", "idiv word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0x0014;  // AX = 20
  helper->cpu_.registers[kDX] = 0x0000;  // DX = 0
  helper->memory_[0x0800] = 0xFC;        // [bx] = -4 (0xFFFC)
  helper->memory_[0x0801] = 0xFF;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0xFFFB);  // AX = 20/(-4) = -5
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x0000);  // DX = 20%(-4) = 0

  // Case 4: Negative dividend, positive divisor
  helper = CPUTestHelper::CreateWithProgram(
      "group3-idiv-word-neg-pos", "idiv word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0xFFEC;  // AX = -20
  helper->cpu_.registers[kDX] =
      0xFFFF;  // DX = -1 (sign extension), so dividend = -20
  helper->memory_[0x0800] = 0x04;  // [bx] = 4
  helper->memory_[0x0801] = 0x00;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0xFFFB);  // AX = -20/4 = -5
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x0000);  // DX = -20%4 = 0

  // Case 5: Negative dividend, negative divisor
  helper = CPUTestHelper::CreateWithProgram(
      "group3-idiv-word-neg-neg", "idiv word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0xFFEC;  // AX = -20
  helper->cpu_.registers[kDX] = 0xFFFF;  // DX = -1, so dividend = -20
  helper->memory_[0x0800] = 0xFC;        // [bx] = -4
  helper->memory_[0x0801] = 0xFF;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0005);  // AX = -20/(-4) = 5
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x0000);  // DX = -20%(-4) = 0

  // Case 6: Division with negative remainder
  helper = CPUTestHelper::CreateWithProgram(
      "group3-idiv-word-neg-remainder", "idiv word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0xFFE9;  // AX = -23
  helper->cpu_.registers[kDX] = 0xFFFF;  // DX = -1, so dividend = -23
  helper->memory_[0x0800] = 0x05;        // [bx] = 5
  helper->memory_[0x0801] = 0x00;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0xFFFC);  // AX = -23/5 = -4
  EXPECT_EQ(helper->cpu_.registers[kDX], 0xFFFD);  // DX = -23%5 = -3

  // Case 7: Large positive dividend
  helper = CPUTestHelper::CreateWithProgram(
      "group3-idiv-word-large-pos", "idiv word [bx]\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->cpu_.registers[kAX] = 0x0002;  // AX = 2
  helper->cpu_.registers[kDX] = 0x0001;  // DX = 1, so dividend = 65538
  helper->memory_[0x0800] = 0x03;        // [bx] = 3
  helper->memory_[0x0801] = 0x00;
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x5556);  // AX = 65538/3 = 21846
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x0000);  // DX = 65538%3 = 0

  // Case 8: Test with register operand
  helper = CPUTestHelper::CreateWithProgram(
      "group3-idiv-word-register", "idiv cx\n");
  helper->cpu_.registers[kAX] = 0x0030;  // AX = 48
  helper->cpu_.registers[kDX] = 0x0000;  // DX = 0
  helper->cpu_.registers[kCX] = 0xFFF4;  // CX = -12
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0xFFFC);  // AX = 48/(-12) = -4
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x0000);  // DX = 48%(-12) = 0
}
