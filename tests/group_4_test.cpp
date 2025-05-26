// filepath: /home/chuan/Projects/yax86/tests/group_4_test.cpp
#include <gtest/gtest.h>

#include "../yax86.h"
#include "./test_helpers.h"

using namespace std;

class Group4Test : public ::testing::Test {};

TEST_F(Group4Test, IncMemoryByte) {
  // Test case for INC r/m8 (Opcode FE /0)
  // Example: INC byte [bx]
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group4-inc-rm8-test", "inc byte [bx]");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;  // Point BX to some memory location
  helper->memory_[0x0800] = 0x01;        // Initial value in memory

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->memory_[0x0800], 0x02);
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kOF, false}, {kAF, false}});

  // Test with overflow
  helper = CPUTestHelper::CreateWithProgram(
      "execute-group4-inc-rm8-overflow-test", "inc byte [bx]");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x7F;  // Max positive signed byte

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x80);  // Should be -128
  helper->CheckFlags({{kZF, false}, {kSF, true}, {kOF, true}, {kAF, true}});

  // Test with zero result
  helper = CPUTestHelper::CreateWithProgram(
      "execute-group4-inc-rm8-zero-test", "inc byte [bx]");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0xFF;  // -1

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x00);
  helper->CheckFlags({{kZF, true}, {kSF, false}, {kOF, false}, {kAF, true}});
}

TEST_F(Group4Test, DecMemoryByte) {
  // Test case for DEC r/m8 (Opcode FE /1)
  // Example: DEC byte [bx]
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group4-dec-rm8-test", "dec byte [bx]");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;  // Point BX to some memory location
  helper->memory_[0x0800] = 0x02;        // Initial value in memory

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->memory_[0x0800], 0x01);
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kOF, false}, {kAF, false}});

  // Test with overflow (to negative)
  helper = CPUTestHelper::CreateWithProgram(
      "execute-group4-dec-rm8-overflow-test", "dec byte [bx]");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x80;  // -128 (Min negative signed byte)

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x7F);  // Should be 127
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kOF, true}, {kAF, true}});

  // Test with zero result
  helper = CPUTestHelper::CreateWithProgram(
      "execute-group4-dec-rm8-zero-test", "dec byte [bx]");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kBX] = 0x0800;
  helper->memory_[0x0800] = 0x01;

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x00);
  helper->CheckFlags({{kZF, true}, {kSF, false}, {kOF, false}, {kAF, false}});
}
