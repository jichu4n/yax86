// filepath: /home/chuan/Projects/yax86/tests/group_4_test.cpp
#include <gtest/gtest.h>

#include "./test_helpers.h"
#include "cpu.h"

using namespace std;

class Group4Test : public ::testing::Test {};

TEST_F(Group4Test, IncMemoryByte) {
  // Test case for INC r/m8 (Opcode FE /0)
  // Example: INC byte [bx]
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group4-inc-rm8-test", "inc byte [bx]");
  helper->cpu_.registers[Y_CPU_DS] = 0;
  helper->cpu_.registers[Y_CPU_BX] =
      0x0800;                            // Point BX to some memory location
  helper->memory_[0x0800] = 0x01;        // Initial value in memory

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->memory_[0x0800], 0x02);
  helper->CheckFlags(
      {{Y_CPU_ZF, false},
       {Y_CPU_SF, false},
       {Y_CPU_OF, false},
       {Y_CPU_AF, false}});

  // Test with overflow
  helper = CPUTestHelper::CreateWithProgram(
      "execute-group4-inc-rm8-overflow-test", "inc byte [bx]");
  helper->cpu_.registers[Y_CPU_DS] = 0;
  helper->cpu_.registers[Y_CPU_BX] = 0x0800;
  helper->memory_[0x0800] = 0x7F;  // Max positive signed byte

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x80);  // Should be -128
  helper->CheckFlags(
      {{Y_CPU_ZF, false},
       {Y_CPU_SF, true},
       {Y_CPU_OF, true},
       {Y_CPU_AF, true}});

  // Test with zero result
  helper = CPUTestHelper::CreateWithProgram(
      "execute-group4-inc-rm8-zero-test", "inc byte [bx]");
  helper->cpu_.registers[Y_CPU_DS] = 0;
  helper->cpu_.registers[Y_CPU_BX] = 0x0800;
  helper->memory_[0x0800] = 0xFF;  // -1

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x00);
  helper->CheckFlags(
      {{Y_CPU_ZF, true},
       {Y_CPU_SF, false},
       {Y_CPU_OF, false},
       {Y_CPU_AF, true}});
}

TEST_F(Group4Test, DecMemoryByte) {
  // Test case for DEC r/m8 (Opcode FE /1)
  // Example: DEC byte [bx]
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-group4-dec-rm8-test", "dec byte [bx]");
  helper->cpu_.registers[Y_CPU_DS] = 0;
  helper->cpu_.registers[Y_CPU_BX] =
      0x0800;                            // Point BX to some memory location
  helper->memory_[0x0800] = 0x02;        // Initial value in memory

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->memory_[0x0800], 0x01);
  helper->CheckFlags(
      {{Y_CPU_ZF, false},
       {Y_CPU_SF, false},
       {Y_CPU_OF, false},
       {Y_CPU_AF, false}});

  // Test with overflow (to negative)
  helper = CPUTestHelper::CreateWithProgram(
      "execute-group4-dec-rm8-overflow-test", "dec byte [bx]");
  helper->cpu_.registers[Y_CPU_DS] = 0;
  helper->cpu_.registers[Y_CPU_BX] = 0x0800;
  helper->memory_[0x0800] = 0x80;  // -128 (Min negative signed byte)

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x7F);  // Should be 127
  helper->CheckFlags(
      {{Y_CPU_ZF, false},
       {Y_CPU_SF, false},
       {Y_CPU_OF, true},
       {Y_CPU_AF, true}});

  // Test with zero result
  helper = CPUTestHelper::CreateWithProgram(
      "execute-group4-dec-rm8-zero-test", "dec byte [bx]");
  helper->cpu_.registers[Y_CPU_DS] = 0;
  helper->cpu_.registers[Y_CPU_BX] = 0x0800;
  helper->memory_[0x0800] = 0x01;

  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->memory_[0x0800], 0x00);
  helper->CheckFlags(
      {{Y_CPU_ZF, true},
       {Y_CPU_SF, false},
       {Y_CPU_OF, false},
       {Y_CPU_AF, false}});
}
