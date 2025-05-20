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
