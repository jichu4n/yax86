#include <gtest/gtest.h>

#include <iomanip>

#include "../yax86.h"
#include "./test_helpers.h"

using namespace std;

// Helper function to test executing a sequence of assembly instructions.
unique_ptr<CPUTestHelper> CreateCPUTestHelper(
    const string& name, const string& asm_code) {
  auto cpu_test_helper = make_unique<CPUTestHelper>(4 * 1024);
  cpu_test_helper->AssembleAndLoadCOM(name, asm_code);
  return cpu_test_helper;
}

// Helper function to test executing a sequence of instructions.
template <typename CPUTestHelperPointerT>
void TestExecuteInstructions(
    const CPUTestHelperPointerT& cpu_test_helper, int num_instructions) {
  CPUState* cpu = &cpu_test_helper->cpu_;
  uint16_t* ip = &cpu->registers[kIP];

  cout << ">> Executing encoded instructions:" << endl;
  for (int i = 0; i < num_instructions; ++i) {
    EncodedInstruction instruction;
    auto status = FetchNextInstruction(cpu, &instruction);
    if (status != kFetchSuccess) {
      throw runtime_error(
          "Failed to fetch instruction at IP: " + to_string(*ip) +
          ", status: " + to_string(status));
    }
    cout << "[" << hex << setw(4) << setfill('0') << (*ip) << "]\t"
         << instruction << endl;
    *ip += instruction.size;
    ExecuteInstruction(cpu, &instruction);
  }
}

// CPU flag and expected value pair.
struct FlagAndValue {
  Flag flag;
  bool value;
};

// Helper function to check the value of a list of flags.
void CheckFlags(const CPUState* cpu, const vector<FlagAndValue>& flags) {
  for (const auto& flag_and_value : flags) {
    EXPECT_EQ(GetFlag(cpu, flag_and_value.flag), flag_and_value.value)
        << "Flag " << GetFlagName(flag_and_value.flag) << " expected to be "
        << (flag_and_value.value ? "set" : "not set") << ", but was "
        << (GetFlag(cpu, flag_and_value.flag) ? "set" : "not set");
  }
}

class ExecuteInstructionTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(ExecuteInstructionTest, ExecuteADDInstructions) {
  auto helper = CreateCPUTestHelper(
      "execute-add-test",
      "add ax, [bx]\n"
      "add [bx], cx\n"
      "add cx, ax\n"
      "add ch, [di+1]\n"
      "add cl, [di-1]\n"
      "add al, 0AAh\n"
      "add ax, 0AAAAh\n");
  helper->cpu_.registers[kDS] = 0;

  // ax = 0002, bx = 0400, memory[0400] = 1234, result = 1236
  helper->cpu_.registers[kAX] = 0x0002;
  helper->cpu_.registers[kBX] = 0x0400;
  helper->memory_[0x400] = 0x34;
  helper->memory_[0x401] = 0x12;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1236);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, true},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});

  // bx = 0400, memory[0400] = 1234, cx = EFFF, result = 0233
  helper->cpu_.registers[kCX] = 0xEFFF;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1236);
  EXPECT_EQ(helper->memory_[0x400], 0x33);
  EXPECT_EQ(helper->memory_[0x401], 0x02);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, true},
                      {kCF, true},
                      {kAF, true},
                      {kOF, false}});

  // cx = EFFF, ax = 1236, result = 0235
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0x0235);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, true},
                      {kCF, true},
                      {kAF, true},
                      {kOF, false}});

  // ch = 02, di+1 = 0501, memory[0501] = AE, result = B0
  helper->cpu_.registers[kDI] = 0x0500;
  helper->memory_[0x0501] = 0xAE;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ((helper->cpu_.registers[kCX] >> 8) & 0xFF, 0xB0);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, true},
                      {kPF, false},
                      {kCF, false},
                      {kAF, true},
                      {kOF, false}});

  // cl = 35, di-1 = 04FF, memory[04FF] = CB, result = 00
  helper->memory_[0x04FF] = 0xCB;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kCX] & 0xFF, 0x00);
  CheckFlags(
      &helper->cpu_, {{kZF, true},
                      {kSF, false},
                      {kPF, true},
                      {kCF, true},
                      {kAF, true},
                      {kOF, false}});

  // al = 55, immediate = AA, result = FF
  helper->cpu_.registers[kAX] = 0x5555;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0xFF);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, true},
                      {kPF, true},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});

  // ax = 5555, immediate = AAAA, result = FFFF
  helper->cpu_.registers[kAX] = 0x5555;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0xFFFF);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, true},
                      {kPF, true},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});
}

TEST_F(ExecuteInstructionTest, ExecuteADCInstructions) {
  auto helper = CreateCPUTestHelper(
      "execute-adc-test",
      "adc ax, [bx]\n"
      "adc [bx], cx\n"
      "adc cx, ax\n"
      "adc ch, [di+1]\n"
      "adc cl, [di-1]\n"
      "adc al, 0AAh\n"
      "adc ax, 0AAAAh\n");
  helper->cpu_.registers[kDS] = 0;

  // ax = 0002, bx = 0400, memory[0400] = 1234, CF = 0, result = 1236
  helper->cpu_.registers[kAX] = 0x0002;
  helper->cpu_.registers[kBX] = 0x0400;
  helper->memory_[0x400] = 0x34;
  helper->memory_[0x401] = 0x12;
  SetFlag(&helper->cpu_, kCF, false);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1236);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, true},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});

  // ax = 0002, bx = 0400, memory[0400] = 1234, CF = 1, result = 1237
  helper->cpu_.registers[kIP] -= 2;
  helper->cpu_.registers[kAX] = 0x0002;
  SetFlag(&helper->cpu_, kCF, true);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1237);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, false},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});

  // bx = 0400, memory[0400] = 1234, cx = EFFF, CF = 0, result = 0233
  helper->cpu_.registers[kCX] = 0xEFFF;
  helper->memory_[0x400] = 0x34;
  helper->memory_[0x401] = 0x12;
  SetFlag(&helper->cpu_, kCF, false);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->memory_[0x400], 0x33);
  EXPECT_EQ(helper->memory_[0x401], 0x02);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, true},
                      {kCF, true},
                      {kAF, true},
                      {kOF, false}});

  // bx = 0400, memory[0400] = 1234, cx = EFFF, CF = 1, result = 0234 in memory
  helper->cpu_.registers[kIP] -= 2;
  helper->memory_[0x400] = 0x34;
  helper->memory_[0x401] = 0x12;
  SetFlag(&helper->cpu_, kCF, true);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->memory_[0x400], 0x34);
  EXPECT_EQ(helper->memory_[0x401], 0x02);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, false},
                      {kCF, true},
                      {kAF, true},
                      {kOF, false}});

  // cx = EFFF, ax = 1237 (from test case 2), CF = 0, result = 0236
  helper->cpu_.registers[kCX] = 0xEFFF;
  helper->cpu_.registers[kAX] = 0x1237;
  SetFlag(&helper->cpu_, kCF, false);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0x0236);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, true},
                      {kCF, true},
                      {kAF, true},
                      {kOF, false}});

  // cx = EFFF, ax = 1237, CF = 1, result = 0237
  helper->cpu_.registers[kIP] -= 2;      // Rewind IP
  helper->cpu_.registers[kCX] = 0xEFFF;  // Reset CX
  SetFlag(&helper->cpu_, kCF, true);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0x0237);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, false},
                      {kCF, true},
                      {kAF, true},
                      {kOF, false}});

  // ch = 02 (from 0x0237), di+1 = 0501, memory[0501] = AE, CF = 0, result = B0
  helper->cpu_.registers[kDI] = 0x0500;
  helper->memory_[0x0501] = 0xAE;
  // CX is 0x0237, so CH is 0x02
  SetFlag(&helper->cpu_, kCF, false);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ((helper->cpu_.registers[kCX] >> 8) & 0xFF, 0xB0);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, true},
                      {kPF, false},
                      {kCF, false},
                      {kAF, true},
                      {kOF, false}});

  // ch = 02, di+1 = 0501, memory[0501] = AE, CF = 1, result = B1
  helper->cpu_.registers[kIP] -= 3;
  helper->cpu_.registers[kCX] =
      (0x02 << 8) | (helper->cpu_.registers[kCX] & 0xFF);
  SetFlag(&helper->cpu_, kCF, true);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ((helper->cpu_.registers[kCX] >> 8) & 0xFF, 0xB1);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, true},
                      {kPF, true},
                      {kCF, false},
                      {kAF, true},
                      {kOF, false}});

  // cl = 37, di-1 = 04FF, memory[04FF] = CB, CF = 0, result = 02
  helper->memory_[0x04FF] = 0xCB;
  SetFlag(&helper->cpu_, kCF, false);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kCX] & 0xFF, 0x02);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, false},
                      {kCF, true},
                      {kAF, true},
                      {kOF, false}});

  // cl = 37, di-1 = 04FF, memory[04FF] = CB, CF = 1, result = 03
  helper->cpu_.registers[kIP] -= 3;
  helper->cpu_.registers[kCX] = (helper->cpu_.registers[kCX] & 0xFF00) | 0x37;
  // CF is already true from previous instruction.
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kCX] & 0xFF, 0x03);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, true},
                      {kCF, true},
                      {kAF, true},
                      {kOF, false}});

  // al = 55, immediate = AA, CF = 0, result = FF
  helper->cpu_.registers[kAX] = 0x5555;
  SetFlag(&helper->cpu_, kCF, false);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0xFF);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, true},
                      {kPF, true},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});

  // al = 55, immediate = AA, CF = 1, result = 00
  helper->cpu_.registers[kIP] -= 2;
  helper->cpu_.registers[kAX] = 0x5555;
  SetFlag(&helper->cpu_, kCF, true);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x00);
  CheckFlags(
      &helper->cpu_, {{kZF, true},
                      {kSF, false},
                      {kPF, true},
                      {kCF, true},
                      {kAF, true},
                      {kOF, false}});

  // ax = 5555, immediate = AAAA, CF = 0, result = FFFF
  helper->cpu_.registers[kAX] = 0x5555;
  SetFlag(&helper->cpu_, kCF, false);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0xFFFF);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, true},
                      {kPF, true},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});

  // ax = 5555, immediate = AAAA, CF = 1 (previous was true), result = 0000
  helper->cpu_.registers[kIP] -= 3;
  helper->cpu_.registers[kAX] = 0x5555;
  SetFlag(&helper->cpu_, kCF, true);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0000);
  CheckFlags(
      &helper->cpu_, {{kZF, true},
                      {kSF, false},
                      {kPF, true},
                      {kCF, true},
                      {kAF, true},
                      {kOF, false}});
}

TEST_F(ExecuteInstructionTest, ExecuteINCInstructions) {
  auto helper = CreateCPUTestHelper(
      "execute-inc-test",
      "inc ax\n"
      "inc cx\n"
      "inc dx\n"
      "inc bx\n"
      "inc sp\n"
      "inc bp\n"
      "inc si\n"
      "inc di\n");
  helper->cpu_.registers[kDS] = 0;

  // Test incrementing AX from 0x0000 to 0x0001, CF flag should remain unchanged
  helper->cpu_.registers[kAX] = 0x0000;
  // Set CF flag to verify INC doesn't change it
  SetFlag(&helper->cpu_, kCF, true);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0001);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, false},
                      {kCF, true},  // CF unchanged
                      {kAF, false},
                      {kOF, false}});

  // Test incrementing CX from 0xFFFF to 0x0000 (overflow)
  helper->cpu_.registers[kCX] = 0xFFFF;
  // Reset CF flag to verify INC doesn't change it
  SetFlag(&helper->cpu_, kCF, false);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0x0000);
  CheckFlags(
      &helper->cpu_, {{kZF, true},
                      {kSF, false},
                      {kPF, true},
                      {kCF, false},  // CF unchanged
                      {kAF, true},
                      {kOF, false}});

  // Test incrementing DX from 0x7FFF to 0x8000 (sign change)
  helper->cpu_.registers[kDX] = 0x7FFF;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x8000);
  CheckFlags(
      &helper->cpu_,
      {{kZF, false},
       {kSF, true},  // Sign changed to negative
       {kPF, true},
       {kCF, false},  // CF unchanged
       {kAF, true},
       {kOF, true}});  // Overflow because sign changed incorrectly

  // Test incrementing BX (regular case)
  helper->cpu_.registers[kBX] = 0x1234;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kBX], 0x1235);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, true},
                      {kCF, false},  // CF unchanged
                      {kAF, false},
                      {kOF, false}});

  // Test incrementing SP (regular case)
  helper->cpu_.registers[kSP] = 0x2000;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kSP], 0x2001);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, false},
                      {kCF, false},  // CF unchanged
                      {kAF, false},
                      {kOF, false}});

  // Test incrementing BP (regular case)
  helper->cpu_.registers[kBP] = 0x3000;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kBP], 0x3001);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, false},
                      {kCF, false},  // CF unchanged
                      {kAF, false},
                      {kOF, false}});

  // Test incrementing SI (regular case)
  helper->cpu_.registers[kSI] = 0x4000;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x4001);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, false},
                      {kCF, false},  // CF unchanged
                      {kAF, false},
                      {kOF, false}});

  // Test incrementing DI (regular case)
  helper->cpu_.registers[kDI] = 0x5000;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x5001);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, false},
                      {kCF, false},  // CF unchanged
                      {kAF, false},
                      {kOF, false}});
}

TEST_F(ExecuteInstructionTest, ExecuteSUBInstructions) {
  auto helper = CreateCPUTestHelper(
      "execute-sub-test",
      "sub ax, [bx]\n"      // Reg - Mem16
      "sub [bx], cx\n"      // Mem16 - Reg
      "sub cx, ax\n"        // Reg - Reg
      "sub ch, [di+1]\n"    // Reg8_high - Mem8
      "sub cl, [di-1]\n"    // Reg8_low - Mem8
      "sub al, 0AAh\n"      // AL - Imm8
      "sub ax, 0AAAAh\n");  // AX - Imm16
  helper->cpu_.registers[kDS] = 0;

  // Test 1: sub ax, [bx]
  // ax = 0x1236, bx = 0x0400, memory[0x0400] = 0x0002. Result ax = 0x1234
  helper->cpu_.registers[kAX] = 0x1236;
  helper->cpu_.registers[kBX] = 0x0400;
  helper->memory_[0x0400] = 0x02;  // LSB
  helper->memory_[0x0401] = 0x00;  // MSB
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1234);
  // Flags: ZF=0, SF=0, PF=0 (0x34 is odd), CF=0, AF=0, OF=0
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, false},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});

  // Test 2: sub [bx], cx
  // memory[0x0400] = 0x1236 (set it), cx = 0x0002. Result memory[0x0400] =
  // 0x1234 bx is still 0x0400. AX is 0x1234.
  helper->memory_[0x0400] = 0x36;
  helper->memory_[0x0401] = 0x12;
  helper->cpu_.registers[kCX] = 0x0002;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->memory_[0x0400], 0x34);
  EXPECT_EQ(helper->memory_[0x0401], 0x12);
  // Flags: ZF=0, SF=0, PF=0, CF=0, AF=0, OF=0
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, false},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});

  // Test 3: sub cx, ax
  // cx = 0x1236, ax = 0x0002 (ax is 0x1234 from test 1, reset it). Result cx =
  // 0x1234
  helper->cpu_.registers[kCX] = 0x1236;  // CX was 0x0002
  helper->cpu_.registers[kAX] = 0x0002;  // AX was 0x1234
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0x1234);
  // Flags: ZF=0, SF=0, PF=0, CF=0, AF=0, OF=0
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, false},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});

  // Test 4: sub ch, [di+1]
  // cx is 0x1234, so ch = 0x12. di = 0x0500, memory[0x0501] = 0x02. Result ch =
  // 0x10 CX becomes 0x1034. AX is 0x0002.
  helper->cpu_.registers[kDI] = 0x0500;
  helper->memory_[0x0501] = 0x02;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ((helper->cpu_.registers[kCX] >> 8) & 0xFF, 0x10);
  // Flags: ZF=0, SF=0, PF=0 (0x10 is odd), CF=0, AF=0, OF=0
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, false},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});

  // Test 5: sub cl, [di-1]
  // cx is 0x1034, so cl = 0x34. di-1 = 0x04FF, memory[0x04FF] = 0x35. Result cl
  // = 0xFF CX becomes 0x10FF. AX is 0x0002.
  helper->memory_[0x04FF] = 0x35;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kCX] & 0xFF, 0xFF);
  // Flags: ZF=0, SF=1, PF=1 (0xFF is even), CF=1, AF=1, OF=0
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, true},
                      {kPF, true},
                      {kCF, true},
                      {kAF, true},
                      {kOF, false}});

  // Test 6: sub al, 0AAh
  // AX is 0x0002. Set AL to 0x55. AX becomes 0x0055.
  // 0x55 - 0xAA = 0xAB. AL=0xAB. AX=0x00AB.
  helper->cpu_.registers[kAX] = (helper->cpu_.registers[kAX] & 0xFF00) | 0x55;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0xAB);
  // Flags: ZF=0, SF=1, PF=0 (0xAB is odd), CF=1, AF=1, OF=1 (pos - neg =
  // neg_result)
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, true},
                      {kPF, false},
                      {kCF, true},
                      {kAF, true},
                      {kOF, true}});

  // Test 7: sub ax, 0AAAAh
  // Set ax = 0xBBBB. 0xBBBB - 0xAAAA = 0x1111.
  // AX was 0x00AB. CX is 0x10FF.
  helper->cpu_.registers[kAX] = 0xBBBB;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1111);
  // Flags: ZF=0, SF=0, PF=1 (0x11 is even), CF=0, AF=0, OF=0
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, true},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});
}

TEST_F(ExecuteInstructionTest, ExecuteSBBInstructions) {
  auto helper = CreateCPUTestHelper(
      "execute-sbb-test",
      "sbb ax, [bx]\n"      // Reg - Mem16 - CF
      "sbb [bx], cx\n"      // Mem16 - Reg - CF
      "sbb cx, ax\n"        // Reg - Reg - CF
      "sbb ch, [di+1]\n"    // Reg8_high - Mem8 - CF
      "sbb cl, [di-1]\n"    // Reg8_low - Mem8 - CF
      "sbb al, 0AAh\n"      // AL - Imm8 - CF
      "sbb ax, 0AAAAh\n");  // AX - Imm16 - CF
  helper->cpu_.registers[kDS] = 0;

  // Test 1: sbb ax, [bx] with CF = 0
  // ax = 0x1236, bx = 0x0400, memory[0x0400] = 0x0002, CF = 0. Result ax =
  // 0x1234
  helper->cpu_.registers[kAX] = 0x1236;
  helper->cpu_.registers[kBX] = 0x0400;
  helper->memory_[0x0400] = 0x02;  // LSB
  helper->memory_[0x0401] = 0x00;  // MSB
  SetFlag(&helper->cpu_, kCF, false);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1234);
  // Flags: ZF=0, SF=0, PF=0 (0x34 is odd), CF=0, AF=1, OF=0
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, false},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});

  // Test 1b: sbb ax, [bx] with CF = 1
  // ax = 0x1236, bx = 0x0400, memory[0x0400] = 0x0002, CF = 1. Result ax =
  // 0x1233
  helper->cpu_.registers[kIP] -= 2;  // Rewind IP to rerun the instruction
  helper->cpu_.registers[kAX] = 0x1236;
  SetFlag(&helper->cpu_, kCF, true);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1233);
  // Flags: ZF=0, SF=0, PF=1 (0x33 is even parity), CF=0, AF=1, OF=0
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, true},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});

  // Test 2: sbb [bx], cx with CF = 0
  // memory[0x0400] = 0x1236, cx = 0x0002, CF = 0. Result memory[0x0400] =
  // 0x1234
  helper->memory_[0x0400] = 0x36;
  helper->memory_[0x0401] = 0x12;
  helper->cpu_.registers[kCX] = 0x0002;
  SetFlag(&helper->cpu_, kCF, false);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->memory_[0x0400], 0x34);
  EXPECT_EQ(helper->memory_[0x0401], 0x12);
  // Flags: ZF=0, SF=0, PF=0, CF=0, AF=1, OF=0
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, false},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});

  // Test 2b: sbb [bx], cx with CF = 1
  // memory[0x0400] = 0x1236, cx = 0x0002, CF = 1. Result memory[0x0400] =
  // 0x1233
  helper->cpu_.registers[kIP] -= 2;  // Rewind IP
  helper->memory_[0x0400] = 0x36;
  helper->memory_[0x0401] = 0x12;
  SetFlag(&helper->cpu_, kCF, true);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->memory_[0x0400], 0x33);
  EXPECT_EQ(helper->memory_[0x0401], 0x12);
  // Flags: ZF=0, SF=0, PF=1, CF=0, AF=1, OF=0
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, true},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});

  // Test 3: sbb cx, ax with CF = 0
  // cx = 0x1236, ax = 0x0002, CF = 0. Result cx = 0x1234
  helper->cpu_.registers[kCX] = 0x1236;
  helper->cpu_.registers[kAX] = 0x0002;
  SetFlag(&helper->cpu_, kCF, false);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0x1234);
  // Flags: ZF=0, SF=0, PF=0, CF=0, AF=1, OF=0
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, false},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});

  // Test 3b: sbb cx, ax with CF = 1
  // cx = 0x1236, ax = 0x0002, CF = 1. Result cx = 0x1233
  helper->cpu_.registers[kIP] -= 2;  // Rewind IP
  helper->cpu_.registers[kCX] = 0x1236;
  SetFlag(&helper->cpu_, kCF, true);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0x1233);
  // Flags: ZF=0, SF=0, PF=1, CF=0, AF=0, OF=0
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, true},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});

  // Test 4: sbb ch, [di+1] with CF = 0
  // cx = 0x1234, di = 0x0500, memory[0x0501] = 0x02, CF = 0. Result ch = 0x10
  helper->cpu_.registers[kCX] = 0x1234;
  helper->cpu_.registers[kDI] = 0x0500;
  helper->memory_[0x0501] = 0x02;
  SetFlag(&helper->cpu_, kCF, false);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ((helper->cpu_.registers[kCX] >> 8) & 0xFF, 0x10);
  // Flags: ZF=0, SF=0, PF=0, CF=0, AF=1, OF=0
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, false},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});

  // Test 4b: sbb ch, [di+1] with CF = 1
  // cx = 0x1234, di = 0x0500, memory[0x0501] = 0x02, CF = 1. Result ch = 0x0F
  helper->cpu_.registers[kIP] -= 3;  // Rewind IP
  helper->cpu_.registers[kCX] = 0x1234;
  SetFlag(&helper->cpu_, kCF, true);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ((helper->cpu_.registers[kCX] >> 8) & 0xFF, 0x0F);
  // Flags: ZF=0, SF=0, PF=1, CF=0, AF=1, OF=0
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, true},
                      {kCF, false},
                      {kAF, true},
                      {kOF, false}});

  // Test 5: sbb cl, [di-1] with CF = 0
  // Set cx to 0x0F34, di-1 = 0x04FF, memory[0x04FF] = 0x35, CF = 0. Result cl =
  // 0xFF
  helper->cpu_.registers[kCX] = (0x0F << 8) | 0x34;
  helper->memory_[0x04FF] = 0x35;
  SetFlag(&helper->cpu_, kCF, false);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kCX] & 0xFF, 0xFF);
  // Flags: ZF=0, SF=1, PF=1, CF=1, AF=1, OF=0
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, true},
                      {kPF, true},
                      {kCF, true},
                      {kAF, true},
                      {kOF, false}});

  // Test 5b: sbb cl, [di-1] with CF = 1
  // Set cx to 0x0F34, di-1 = 0x04FF, memory[0x04FF] = 0x35, CF = 1. Result cl =
  // 0xFE
  helper->cpu_.registers[kIP] -= 3;  // Rewind IP
  helper->cpu_.registers[kCX] = (0x0F << 8) | 0x34;
  SetFlag(&helper->cpu_, kCF, true);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kCX] & 0xFF, 0xFE);
  // Flags: ZF=0, SF=1, PF=0, CF=1, AF=1, OF=0
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, true},
                      {kPF, false},
                      {kCF, true},
                      {kAF, true},
                      {kOF, false}});

  // Test 6: sbb al, 0AAh with CF = 0
  // ax = 0x0055, CF = 0. Result al = 0xAB
  helper->cpu_.registers[kAX] = 0x0055;
  SetFlag(&helper->cpu_, kCF, false);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0xAB);
  // Flags: ZF=0, SF=1, PF=0, CF=1, AF=1, OF=1 (pos - neg = neg_result)
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, true},
                      {kPF, false},
                      {kCF, true},
                      {kAF, true},
                      {kOF, true}});

  // Test 6b: sbb al, 0AAh with CF = 1
  // ax = 0x0055, CF = 1. Result al = 0xAA
  helper->cpu_.registers[kIP] -= 2;  // Rewind IP
  helper->cpu_.registers[kAX] = 0x0055;
  SetFlag(&helper->cpu_, kCF, true);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0xAA);
  // Flags: ZF=0, SF=1, PF=0, CF=1, AF=1, OF=1
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, true},
                      {kPF, true},
                      {kCF, true},
                      {kAF, true},
                      {kOF, true}});

  // Test 7: sbb ax, 0AAAAh with CF = 0
  // ax = 0xBBBB, CF = 0. Result ax = 0x1111
  helper->cpu_.registers[kAX] = 0xBBBB;
  SetFlag(&helper->cpu_, kCF, false);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1111);
  // Flags: ZF=0, SF=0, PF=1, CF=0, AF=0, OF=0
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, true},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});

  // Test 7b: sbb ax, 0AAAAh with CF = 1
  // ax = 0xBBBB, CF = 1. Result ax = 0x1110
  helper->cpu_.registers[kIP] -= 3;  // Rewind IP
  helper->cpu_.registers[kAX] = 0xBBBB;
  SetFlag(&helper->cpu_, kCF, true);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1110);
  // Flags: ZF=0, SF=0, PF=0, CF=0, AF=0, OF=0
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, false},
                      {kCF, false},
                      {kAF, false},
                      {kOF, false}});
}

TEST_F(ExecuteInstructionTest, ExecuteDECInstructions) {
  auto helper = CreateCPUTestHelper(
      "execute-dec-test",
      "dec ax\n"
      "dec cx\n"
      "dec dx\n"
      "dec bx\n"
      "dec sp\n"
      "dec bp\n"
      "dec si\n"
      "dec di\n");
  helper->cpu_.registers[kDS] = 0;

  // Test decrementing AX from 0x0001 to 0x0000, CF flag should remain unchanged
  helper->cpu_.registers[kAX] = 0x0001;
  // Set CF flag to verify DEC doesn't change it
  SetFlag(&helper->cpu_, kCF, true);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0000);
  CheckFlags(
      &helper->cpu_, {{kZF, true},
                      {kSF, false},
                      {kPF, true},
                      {kCF, true},  // CF unchanged
                      {kAF, false},
                      {kOF, false}});

  // Test decrementing CX from 0x0000 to 0xFFFF (underflow)
  helper->cpu_.registers[kCX] = 0x0000;
  // Reset CF flag to verify DEC doesn't change it
  SetFlag(&helper->cpu_, kCF, false);
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0xFFFF);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, true},
                      {kPF, true},
                      {kCF, false},  // CF unchanged
                      {kAF, true},
                      {kOF, false}});

  // Test decrementing DX from 0x8000 to 0x7FFF (sign change from neg to pos)
  helper->cpu_.registers[kDX] = 0x8000;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x7FFF);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},  // Sign changed to positive
                      {kPF, true},
                      {kCF, false},  // CF unchanged
                      {kAF, true},
                      {kOF, true}});  // Overflow because sign changed from neg
                                      // to pos on subtraction

  // Test decrementing BX (regular case)
  helper->cpu_.registers[kBX] = 0x1235;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kBX], 0x1234);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, false},
                      {kCF, false},  // CF unchanged
                      {kAF, false},
                      {kOF, false}});

  // Test decrementing SP (regular case)
  helper->cpu_.registers[kSP] = 0x2001;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kSP], 0x2000);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, true},
                      {kCF, false},  // CF unchanged
                      {kAF, false},
                      {kOF, false}});

  // Test decrementing BP (regular case)
  helper->cpu_.registers[kBP] = 0x3001;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kBP], 0x3000);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, true},
                      {kCF, false},  // CF unchanged
                      {kAF, false},
                      {kOF, false}});

  // Test decrementing SI (regular case)
  helper->cpu_.registers[kSI] = 0x4001;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x4000);
  CheckFlags(
      &helper->cpu_, {{kZF, false},
                      {kSF, false},
                      {kPF, true},
                      {kCF, false},  // CF unchanged
                      {kAF, false},
                      {kOF, false}});

  // Test decrementing DI (regular case)
  helper->cpu_.registers[kDI] = 0x5002;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x5001);
  CheckFlags(
      &helper->cpu_, {{kZF, false},  // Result is 0x5000
                      {kSF, false},
                      {kPF, false},
                      {kCF, false},  // CF unchanged
                      {kAF, false},
                      {kOF, false}});
}
