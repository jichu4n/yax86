#include <gtest/gtest.h>

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
    cout << "  " << instruction << endl;
    ExecuteInstruction(cpu, &instruction);
    *ip += instruction.size;
  }
}

class ExecuteInstructionTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(ExecuteInstructionTest, ExecuteAddInstruction) {
  auto helper = CreateCPUTestHelper(
      "execute-add-test",
      "add ax, [bx]\n"
      "add [bx], cx\n"
      "add cx, ax\n"
      "add ch, [di+1]\n"
      "add cl, [di-1]\n");

  helper->cpu_.registers[kDS] = 0;
  // ax = 0002, bx = 0400, memory[0400] = 1234, result = 1236
  helper->cpu_.registers[kAX] = 0x0002;
  helper->cpu_.registers[kBX] = 0x0400;
  helper->memory_[0x400] = 0x34;
  helper->memory_[0x401] = 0x12;

  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1236);
  EXPECT_EQ(GetFlag(&helper->cpu_, kCF), false);

  // bx = 0400, memory[0400] = 1234, cx = 0xEFFF, result = 0233
  helper->cpu_.registers[kCX] = 0xEFFF;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1236);
  EXPECT_EQ(helper->memory_[0x400], 0x33);
  EXPECT_EQ(helper->memory_[0x401], 0x02);
  EXPECT_EQ(GetFlag(&helper->cpu_, kCF), true);

  // cx = EFFF, ax = 1236, result = 0235
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0x0235);
  EXPECT_EQ(GetFlag(&helper->cpu_, kCF), true);

  // ch = 02, di+1 = 0501, memory[0501] = ae, result = b0
  helper->cpu_.registers[kDI] = 0x0500;
  helper->memory_[0x0501] = 0xAE;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ((helper->cpu_.registers[kCX] >> 8) & 0xFF, 0xB0);
  EXPECT_EQ(GetFlag(&helper->cpu_, kCF), false);

  // cl = 35, di-1 = 04FF, memory[04FF] = cb, result = 00
  helper->memory_[0x04FF] = 0xCB;
  TestExecuteInstructions(helper, 1);
  EXPECT_EQ(helper->cpu_.registers[kCX] & 0xFF, 0x00);
  EXPECT_EQ(GetFlag(&helper->cpu_, kCF), true);
}
