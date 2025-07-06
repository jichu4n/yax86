#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "./test_helpers.h"
#include "cpu.h"

using namespace std;

// Helper function to test decoding a sequence of assembly instructions.
vector<Instruction> TestFetchInstructions(
    const string& name, const string& asm_code) {
  CPUTestHelper cpu_test_helper(4 * 1024);
  size_t machine_code_size =
      cpu_test_helper.AssembleAndLoadProgram(name, asm_code);
  CPUState* cpu = &cpu_test_helper.cpu_;
  uint16_t* ip = &cpu->registers[kIP];

  // Fetch instructions until we reach the end of the machine code
  cout << ">> Reading encoded instructions:" << endl;
  vector<Instruction> instructions;
  while (*ip < kCOMFileLoadOffset + machine_code_size) {
    Instruction instruction;
    auto status = FetchNextInstruction(cpu, &instruction);
    if (status != kFetchSuccess) {
      throw runtime_error(
          "Failed to fetch instruction at IP: " + to_string(*ip) +
          ", status: " + to_string(status));
    }
    instructions.push_back(instruction);
    cout << "  " << instruction << endl;
    *ip += instruction.size;
  }

  return instructions;
}

class FetchNextInstructionTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

// Test assembling a simple MOV instruction.
TEST_F(FetchNextInstructionTest, CanAssembleAndReadBackMachineCode) {
  auto machine_code = Assemble("assemble-test", "mov ax, 0x1234");
  EXPECT_EQ(machine_code, (vector<uint8_t>{0xb8, 0x34, 0x12}));
}

// Test fetching a single MOV instruction.
TEST_F(FetchNextInstructionTest, FetchSingleMOVInstruction) {
  auto instructions = TestFetchInstructions("fetch-test", "mov ax, 0x1234");
  ASSERT_EQ(instructions.size(), 1);
  EXPECT_EQ(instructions[0].opcode, 0xb8);  // MOV AX, imm16
  EXPECT_EQ(instructions[0].has_mod_rm, false);
  EXPECT_EQ(instructions[0].displacement_size, 0);
  EXPECT_EQ(instructions[0].immediate_size, 2);
  EXPECT_EQ(instructions[0].immediate[0], 0x34);
  EXPECT_EQ(instructions[0].immediate[1], 0x12);
}

// Test fetching a sequence of simple MOV instructions.
TEST_F(FetchNextInstructionTest, FetchMultipleMOVInstructions) {
  auto instructions = TestFetchInstructions(
      "fetch-multiple-test", "mov ax, 0x1234\nmov bx, 0x5678");
  ASSERT_EQ(instructions.size(), 2);

  EXPECT_EQ(instructions[0].opcode, 0xb8);  // MOV AX, imm16
  EXPECT_EQ(instructions[0].has_mod_rm, false);
  EXPECT_EQ(instructions[0].displacement_size, 0);
  EXPECT_EQ(instructions[0].immediate_size, 2);
  EXPECT_EQ(instructions[0].immediate[0], 0x34);
  EXPECT_EQ(instructions[0].immediate[1], 0x12);

  EXPECT_EQ(instructions[1].opcode, 0xbb);  // MOV BX, imm16
  EXPECT_EQ(instructions[1].has_mod_rm, false);
  EXPECT_EQ(instructions[1].displacement_size, 0);
  EXPECT_EQ(instructions[1].immediate_size, 2);
  EXPECT_EQ(instructions[1].immediate[0], 0x78);
  EXPECT_EQ(instructions[1].immediate[1], 0x56);
}

// Test fetching a variety of MOV instructions with different source /
// destinations and immediate sizes.
TEST_F(FetchNextInstructionTest, FetchMOVInstructions) {
  auto instructions = TestFetchInstructions(
      "fetch-mov-test",
      // MOV r16, imm16
      "mov ax, 0x1234\n"
      // MOV r8, imm8
      "mov bl, 0x56\n"
      // MOV r16, r16
      "mov cx, dx\n"
      // MOV r8, r8
      "mov dh, al\n"
      // MOV [r16], r16
      "mov [bx], ax\n"
      // MOV [r16+disp8], r8
      "mov [si+2], cl\n"
      // MOV r16, [r16+disp16]
      "mov bp, [di+0x1234]\n"
      // MOV [disp16], r16
      "mov [0x5678], dx\n"
      // MOV byte [r16], imm8
      "mov byte [bp], 0x12\n"
      // MOV word [r16+r16], imm16
      "mov word [bx+si], 0x3456\n"
      // MOV sreg, r16
      "mov es, ax\n"
      // MOV r16, sreg
      "mov bx, ds");

  ASSERT_EQ(instructions.size(), 12);
}

// Test fetching a sequence of instructions with prefixes.
TEST_F(FetchNextInstructionTest, FetchInstructionsWithPrefixes) {
  auto instructions = TestFetchInstructions(
      "fetch-prefixes-test",
      // REP prefix
      "rep movsb\n"
      // REPNE prefix
      "repne movsb\n"
      // LOCK prefix
      "lock add [bx], ax\n"
      // Multiple prefixes
      "rep lock mov ds, [bx]\n"
      // CS segment override prefix
      "cs mov ax, [bx]\n"
      // ES segment override prefix with REP
      "rep es mov ax, [bx]\n"
      // SS segment override prefix with REPNE
      "repne ss mov ax, [bx]\n"
      // DS segment override prefix with LOCK
      "lock ds mov ax, [bx]\n");

  ASSERT_EQ(instructions.size(), 8);
  // REP prefix
  EXPECT_EQ(instructions[0].prefix_size, 1);
  EXPECT_EQ(instructions[0].prefix[0], 0xf3);
  // REPNE prefix
  EXPECT_EQ(instructions[1].prefix_size, 1);
  EXPECT_EQ(instructions[1].prefix[0], 0xf2);
  // LOCK prefix
  EXPECT_EQ(instructions[2].prefix_size, 1);
  EXPECT_EQ(instructions[2].prefix[0], 0xf0);
  // Multiple prefixes
  EXPECT_EQ(instructions[3].prefix_size, 2);
  EXPECT_EQ(instructions[3].prefix[0], 0xf3);
  EXPECT_EQ(instructions[3].prefix[1], 0xf0);
  // CS segment override prefix
  EXPECT_EQ(instructions[4].prefix_size, 1);
  EXPECT_EQ(instructions[4].prefix[0], 0x2e);
  // CS segment override prefix with REP
  EXPECT_EQ(instructions[5].prefix_size, 2);
  EXPECT_EQ(instructions[5].prefix[0], 0xf3);
  EXPECT_EQ(instructions[5].prefix[1], 0x26);
  // SS segment override prefix with REPNE
  EXPECT_EQ(instructions[6].prefix_size, 2);
  EXPECT_EQ(instructions[6].prefix[0], 0xf2);
  EXPECT_EQ(instructions[6].prefix[1], 0x36);
  // DS segment override prefix with LOCK
  EXPECT_EQ(instructions[7].prefix_size, 2);
  EXPECT_EQ(instructions[7].prefix[0], 0xf0);
  EXPECT_EQ(instructions[7].prefix[1], 0x3e);
}

// Test fetching a sequence of instructions with 0, 1, and 2 displacement bytes.
TEST_F(FetchNextInstructionTest, FetchInstructionsWithDisplacement) {
  auto instructions = TestFetchInstructions(
      "fetch-displacement-test",
      // MOV r16, [r16+disp8]
      "mov ax, [bx+2]\n"
      // MOV r16, [r16+disp16]
      "mov bx, [si+0x1234]\n"
      // MOV [r16+disp8], r8
      "mov [di+3], cl\n"
      // MOV [r16+disp16], r16
      "mov [bp+0x5678], dx\n"
      // MOV [r16], r16
      "mov ax, [bx]\n");

  ASSERT_EQ(instructions.size(), 5);
  // MOV r16, [r16+disp8]
  EXPECT_EQ(instructions[0].displacement_size, 1);
  EXPECT_EQ(instructions[0].displacement[0], 2);
  // MOV r16, [r16+disp16]
  EXPECT_EQ(instructions[1].displacement_size, 2);
  EXPECT_EQ(instructions[1].displacement[0], 0x34);
  EXPECT_EQ(instructions[1].displacement[1], 0x12);
  // MOV [r16+disp8], r8
  EXPECT_EQ(instructions[2].displacement_size, 1);
  EXPECT_EQ(instructions[2].displacement[0], 3);
  // MOV [r16+disp16], r16
  EXPECT_EQ(instructions[3].displacement_size, 2);
  EXPECT_EQ(instructions[3].displacement[0], 0x78);
  EXPECT_EQ(instructions[3].displacement[1], 0x56);
  // MOV [r16], r16
  EXPECT_EQ(instructions[4].displacement_size, 0);
}

// Test 0xF6 and 0xF7 instructions with immediate data.
TEST_F(FetchNextInstructionTest, FetchF6F7Instructions) {
  auto instructions = TestFetchInstructions(
      "fetch-f6f7-test",
      // NOT r/m8
      "not bl\n"
      // MUL r/m16
      "mul cx\n"
      // TEST r/m8, imm8
      "test byte [bx], 0x01\n"
      // TEST r/m16, imm16
      "test word [si+0x1234], 0x0002\n");

  ASSERT_EQ(instructions.size(), 4);

  // NOT r/m8
  EXPECT_EQ(instructions[0].opcode, 0xf6);
  EXPECT_EQ(instructions[0].has_mod_rm, true);
  EXPECT_EQ(instructions[0].immediate_size, 0);
  // MUL r/m16
  EXPECT_EQ(instructions[1].opcode, 0xf7);
  EXPECT_EQ(instructions[1].has_mod_rm, true);
  EXPECT_EQ(instructions[1].immediate_size, 0);
  // TEST r/m8, imm8
  EXPECT_EQ(instructions[2].opcode, 0xf6);
  EXPECT_EQ(instructions[2].has_mod_rm, true);
  EXPECT_EQ(instructions[2].immediate_size, 1);
  EXPECT_EQ(instructions[2].immediate[0], 0x01);
  // TEST r/m16, imm16
  EXPECT_EQ(instructions[3].opcode, 0xf7);
  EXPECT_EQ(instructions[3].has_mod_rm, true);
  EXPECT_EQ(instructions[3].immediate_size, 2);
  EXPECT_EQ(instructions[3].immediate[0], 0x02);
  EXPECT_EQ(instructions[3].immediate[1], 0x00);
}

// Test fetching JMP and CALL instructions with different immediate sizes.
TEST_F(FetchNextInstructionTest, FetchJmpCallInstructions) {
  auto instructions = TestFetchInstructions(
      "fetch-jmp-call-test",
      // JMP rel16
      "jmp 0x1234\n"
      // CALL rel16
      "call 0x5678\n"
      // JMP ptr16:16
      "jmp 0x9abc:0xdef0\n"
      // CALL ptr16:16
      "call 0x1357:0x2468\n");

  ASSERT_EQ(instructions.size(), 4);

  // JMP rel16
  EXPECT_EQ(instructions[0].opcode, 0xe9);
  EXPECT_EQ(instructions[0].immediate_size, 2);
  EXPECT_EQ(instructions[0].immediate[0], 0x31);
  EXPECT_EQ(instructions[0].immediate[1], 0x11);

  // CALL rel16
  EXPECT_EQ(instructions[1].opcode, 0xe8);
  EXPECT_EQ(instructions[1].immediate_size, 2);
  EXPECT_EQ(instructions[1].immediate[0], 0x72);
  EXPECT_EQ(instructions[1].immediate[1], 0x55);

  // JMP ptr16:16
  EXPECT_EQ(instructions[2].opcode, 0xea);
  EXPECT_EQ(instructions[2].immediate_size, 4);
  EXPECT_EQ(instructions[2].immediate[0], 0xf0);
  EXPECT_EQ(instructions[2].immediate[1], 0xde);
  EXPECT_EQ(instructions[2].immediate[2], 0xbc);
  EXPECT_EQ(instructions[2].immediate[3], 0x9a);
  // CALL ptr16:16
  EXPECT_EQ(instructions[3].opcode, 0x9a);
  EXPECT_EQ(instructions[3].immediate_size, 4);
  EXPECT_EQ(instructions[3].immediate[0], 0x68);
  EXPECT_EQ(instructions[3].immediate[1], 0x24);
  EXPECT_EQ(instructions[3].immediate[2], 0x57);
  EXPECT_EQ(instructions[3].immediate[3], 0x13);
}