#include <gtest/gtest.h>

#include "../yax86.h"
#include "./test_helpers.h"

using namespace std;

class BcdTest : public ::testing::Test {};

TEST_F(BcdTest, AAA_NoAdjustmentNeeded) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-aaa-no-adjustment", "aaa\n");

  // Test case 1: AL = 05, AH = 02, AF = 0
  // Should not adjust since (AL & 0x0F) = 5 <= 9 and AF = 0
  helper->cpu_.registers[kAX] = 0x0205;  // AH = 02, AL = 05
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = 05 & 0x0F = 05, AH unchanged = 02
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0205);
  helper->CheckFlags({{kAF, false}, {kCF, false}});

  // Test case 2: AL = 09, AH = 00, AF = 0
  // Should not adjust since (AL & 0x0F) = 9 <= 9 and AF = 0
  helper->cpu_.registers[kIP] -= 1;      // Rewind IP
  helper->cpu_.registers[kAX] = 0x0009;  // AH = 00, AL = 09
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = 09 & 0x0F = 09, AH unchanged = 00
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0009);
  helper->CheckFlags({{kAF, false}, {kCF, false}});
}

TEST_F(BcdTest, AAA_AdjustmentNeededLowNibbleGreaterThan9) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "test-aaa-adjustment-low-nibble", "aaa\n");

  // Test case 1: AL = 0A, AH = 00, AF = 0
  // Should adjust since (AL & 0x0F) = 10 > 9
  helper->cpu_.registers[kAX] = 0x000A;  // AH = 00, AL = 0A
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = (0A + 6) & 0x0F = 10 & 0x0F = 0, AH = 00 + 1 = 01
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0100);
  helper->CheckFlags({{kAF, true}, {kCF, true}});

  // Test case 2: AL = 1F, AH = 03, AF = 0
  // Should adjust since (AL & 0x0F) = 15 > 9
  helper->cpu_.registers[kIP] -= 1;      // Rewind IP
  helper->cpu_.registers[kAX] = 0x031F;  // AH = 03, AL = 1F
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = (1F + 6) & 0x0F = 25 & 0x0F = 5, AH = 03 + 1 = 04
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0405);
  helper->CheckFlags({{kAF, true}, {kCF, true}});
}

TEST_F(BcdTest, AAA_AdjustmentNeededAFSet) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-aaa-adjustment-af-set", "aaa\n");

  // Test case 1: AL = 02, AH = 01, AF = 1
  // Should adjust since AF = 1, even though (AL & 0x0F) = 2 <= 9
  helper->cpu_.registers[kAX] = 0x0102;  // AH = 01, AL = 02
  SetFlag(&helper->cpu_, kAF, true);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = (02 + 6) & 0x0F = 8 & 0x0F = 8, AH = 01 + 1 = 02
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0208);
  helper->CheckFlags({{kAF, true}, {kCF, true}});

  // Test case 2: AL = 07, AH = 00, AF = 1
  // Should adjust since AF = 1, even though (AL & 0x0F) = 7 <= 9
  helper->cpu_.registers[kIP] -= 1;      // Rewind IP
  helper->cpu_.registers[kAX] = 0x0007;  // AH = 00, AL = 07
  SetFlag(&helper->cpu_, kAF, true);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = (07 + 6) & 0x0F = 13 & 0x0F = 13, AH = 00 + 1 = 01
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x010D);
  helper->CheckFlags({{kAF, true}, {kCF, true}});
}

TEST_F(BcdTest, AAA_AdjustmentWithUpperNibbleClearing) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "test-aaa-upper-nibble-clearing", "aaa\n");

  // Test case 1: AL = 5F, AH = 02, AF = 0
  // Should adjust since (AL & 0x0F) = 15 > 9
  // Upper nibble of AL should be cleared regardless
  helper->cpu_.registers[kAX] = 0x025F;  // AH = 02, AL = 5F
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = (5F + 6) & 0x0F = 65 & 0x0F = 5, AH = 02 + 1 = 03
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0305);
  helper->CheckFlags({{kAF, true}, {kCF, true}});

  // Test case 2: AL = A5, AH = 01, AF = 0 (no adjustment but upper nibble
  // cleared)
  helper->cpu_.registers[kIP] -= 1;      // Rewind IP
  helper->cpu_.registers[kAX] = 0x01A5;  // AH = 01, AL = A5
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = A5 & 0x0F = 5 (no addition since (AL & 0x0F) = 5 <= 9), AH
  // unchanged = 01
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0105);
  helper->CheckFlags({{kAF, false}, {kCF, false}});
}

TEST_F(BcdTest, AAA_EdgeCases) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-aaa-edge-cases", "aaa\n");

  // Test case 1: AL = FF, AH = FF, AF = 0
  // Should adjust since (AL & 0x0F) = 15 > 9
  helper->cpu_.registers[kAX] = 0xFFFF;  // AH = FF, AL = FF
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = (FF + 6) & 0x0F = 105 & 0x0F = 5, AH = FF + 1 = 00 (wraps
  // around)
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0005);
  helper->CheckFlags({{kAF, true}, {kCF, true}});

  // Test case 2: AL = 00, AH = 00, AF = 0
  // Should not adjust since (AL & 0x0F) = 0 <= 9 and AF = 0
  helper->cpu_.registers[kIP] -= 1;      // Rewind IP
  helper->cpu_.registers[kAX] = 0x0000;  // AH = 00, AL = 00
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = 00 & 0x0F = 00, AH unchanged = 00
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0000);
  helper->CheckFlags({{kAF, false}, {kCF, false}});
}

TEST_F(BcdTest, AAA_BothConditionsTrue) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-aaa-both-conditions", "aaa\n");

  // Test case: AL = 3E, AH = 01, AF = 1
  // Should adjust since both (AL & 0x0F) = 14 > 9 AND AF = 1
  helper->cpu_.registers[kAX] = 0x013E;  // AH = 01, AL = 3E
  SetFlag(&helper->cpu_, kAF, true);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = (3E + 6) & 0x0F = 44 & 0x0F = 4, AH = 01 + 1 = 02
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0204);
  helper->CheckFlags({{kAF, true}, {kCF, true}});
}

TEST_F(BcdTest, AAA_TypicalBCDUsage) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "test-aaa-bcd-usage",
      "add al, bl\n"
      "aaa\n");

  // Simulate adding two BCD digits: 7 + 6 = 13
  // This should result in AL = 0D, then AAA should adjust it
  helper->cpu_.registers[kAX] = 0x0007;  // AL = 07
  helper->cpu_.registers[kBX] = 0x0006;  // BL = 06
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  // Execute ADD AL, BL
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x0D);  // AL should be 0D

  // Execute AAA
  helper->ExecuteInstructions(1);

  // Expect AL = (0D + 6) & 0x0F = 13 & 0x0F = 3, AH = 00 + 1 = 01
  // This represents BCD result 13 (1 in AH, 3 in AL)
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0103);
  helper->CheckFlags({{kAF, true}, {kCF, true}});
}

TEST_F(BcdTest, AAS_NoAdjustmentNeeded) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-aas-no-adjustment", "aas\n");

  // Test case 1: AL = 05, AH = 02, AF = 0
  // Should not adjust since (AL & 0x0F) = 5 <= 9 and AF = 0
  helper->cpu_.registers[kAX] = 0x0205;  // AH = 02, AL = 05
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = 05 & 0x0F = 05, AH unchanged = 02
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0205);
  helper->CheckFlags({{kAF, false}, {kCF, false}});

  // Test case 2: AL = 09, AH = 00, AF = 0
  // Should not adjust since (AL & 0x0F) = 9 <= 9 and AF = 0
  helper->cpu_.registers[kIP] -= 1;      // Rewind IP
  helper->cpu_.registers[kAX] = 0x0009;  // AH = 00, AL = 09
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = 09 & 0x0F = 09, AH unchanged = 00
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0009);
  helper->CheckFlags({{kAF, false}, {kCF, false}});
}

TEST_F(BcdTest, AAS_AdjustmentNeededLowNibbleGreaterThan9) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "test-aas-adjustment-low-nibble", "aas\n");

  // Test case 1: AL = 0A, AH = 02, AF = 0
  // Should adjust since (AL & 0x0F) = 10 > 9
  helper->cpu_.registers[kAX] = 0x020A;  // AH = 02, AL = 0A
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = (0A - 6) & 0x0F = 4 & 0x0F = 4, AH = 02 - 1 = 01
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0104);
  helper->CheckFlags({{kAF, true}, {kCF, true}});

  // Test case 2: AL = 1F, AH = 03, AF = 0
  // Should adjust since (AL & 0x0F) = 15 > 9
  helper->cpu_.registers[kIP] -= 1;      // Rewind IP
  helper->cpu_.registers[kAX] = 0x031F;  // AH = 03, AL = 1F
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = (1F - 6) & 0x0F = 19 & 0x0F = 9, AH = 03 - 1 = 02
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0209);
  helper->CheckFlags({{kAF, true}, {kCF, true}});
}

TEST_F(BcdTest, AAS_AdjustmentNeededAFSet) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-aas-adjustment-af-set", "aas\n");

  // Test case 1: AL = 02, AH = 01, AF = 1
  // Should adjust since AF = 1, even though (AL & 0x0F) = 2 <= 9
  helper->cpu_.registers[kAX] = 0x0102;  // AH = 01, AL = 02
  SetFlag(&helper->cpu_, kAF, true);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = (02 - 6) & 0x0F = -4 & 0x0F = 12 & 0x0F = 12, AH = 01 - 1 = 00
  // Note: -4 in uint16_t is 0xFFFC, so (0xFFFC & 0x0F) = 0x0C = 12
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x000C);
  helper->CheckFlags({{kAF, true}, {kCF, true}});

  // Test case 2: AL = 07, AH = 02, AF = 1
  // Should adjust since AF = 1, even though (AL & 0x0F) = 7 <= 9
  helper->cpu_.registers[kIP] -= 1;      // Rewind IP
  helper->cpu_.registers[kAX] = 0x0207;  // AH = 02, AL = 07
  SetFlag(&helper->cpu_, kAF, true);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = (07 - 6) & 0x0F = 1 & 0x0F = 1, AH = 02 - 1 = 01
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0101);
  helper->CheckFlags({{kAF, true}, {kCF, true}});
}

TEST_F(BcdTest, AAS_AdjustmentWithUpperNibbleClearing) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "test-aas-upper-nibble-clearing", "aas\n");

  // Test case 1: AL = 5F, AH = 02, AF = 0
  // Should adjust since (AL & 0x0F) = 15 > 9
  // Upper nibble of AL should be cleared regardless
  helper->cpu_.registers[kAX] = 0x025F;  // AH = 02, AL = 5F
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = (5F - 6) & 0x0F = 59 & 0x0F = 9, AH = 02 - 1 = 01
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0109);
  helper->CheckFlags({{kAF, true}, {kCF, true}});

  // Test case 2: AL = A5, AH = 01, AF = 0 (no adjustment but upper nibble
  // cleared)
  helper->cpu_.registers[kIP] -= 1;      // Rewind IP
  helper->cpu_.registers[kAX] = 0x01A5;  // AH = 01, AL = A5
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = A5 & 0x0F = 5 (no subtraction since (AL & 0x0F) = 5 <= 9), AH
  // unchanged = 01
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0105);
  helper->CheckFlags({{kAF, false}, {kCF, false}});
}

TEST_F(BcdTest, AAS_EdgeCases) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-aas-edge-cases", "aas\n");

  // Test case 1: AL = FF, AH = FF, AF = 0
  // Should adjust since (AL & 0x0F) = 15 > 9
  helper->cpu_.registers[kAX] = 0xFFFF;  // AH = FF, AL = FF
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = (FF - 6) & 0x0F = F9 & 0x0F = 9, AH = FF - 1 = FE
  EXPECT_EQ(helper->cpu_.registers[kAX], 0xFE09);
  helper->CheckFlags({{kAF, true}, {kCF, true}});

  // Test case 2: AL = 00, AH = 00, AF = 0
  // Should not adjust since (AL & 0x0F) = 0 <= 9 and AF = 0
  helper->cpu_.registers[kIP] -= 1;      // Rewind IP
  helper->cpu_.registers[kAX] = 0x0000;  // AH = 00, AL = 00
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = 00 & 0x0F = 00, AH unchanged = 00
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0000);
  helper->CheckFlags({{kAF, false}, {kCF, false}});
}

TEST_F(BcdTest, AAS_BothConditionsTrue) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-aas-both-conditions", "aas\n");

  // Test case: AL = 3E, AH = 01, AF = 1
  // Should adjust since both (AL & 0x0F) = 14 > 9 AND AF = 1
  helper->cpu_.registers[kAX] = 0x013E;  // AH = 01, AL = 3E
  SetFlag(&helper->cpu_, kAF, true);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Expect AL = (3E - 6) & 0x0F = 38 & 0x0F = 8, AH = 01 - 1 = 00
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0008);
  helper->CheckFlags({{kAF, true}, {kCF, true}});
}

TEST_F(BcdTest, AAS_TypicalBCDUsage) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "test-aas-bcd-usage",
      "sub al, bl\n"
      "aas\n");

  // Simulate subtracting two BCD digits: 3 - 6 = -3
  // This should result in AL with borrow, then AAS should adjust it
  helper->cpu_.registers[kAX] = 0x0003;  // AL = 03
  helper->cpu_.registers[kBX] = 0x0006;  // BL = 06
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  // Execute SUB AL, BL
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX] & 0xFF,
      0xFD);  // AL should be FD (3-6 = -3 = 0xFD)

  // Execute AAS
  helper->ExecuteInstructions(1);

  // Expect AL = (FD - 6) & 0x0F = F7 & 0x0F = 7, AH = 00 - 1 = FF
  // This represents BCD result -3 with borrow (FF in AH, 7 in AL)
  EXPECT_EQ(helper->cpu_.registers[kAX], 0xFF07);
  helper->CheckFlags({{kAF, true}, {kCF, true}});
}

TEST_F(BcdTest, AAM_StandardDecimalBase) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "test-aam-standard-decimal", "aam 0ah\n");

  // Test case 1: AL = 0x17 (23 decimal), base = 10
  // Should result in AH = 2, AL = 3 (23 / 10 = 2 remainder 3)
  helper->cpu_.registers[kAX] = 0x0017;  // AH = 00, AL = 17
  SetFlag(&helper->cpu_, kCF, true);  // Set some flags to test they're changed
  SetFlag(&helper->cpu_, kOF, true);

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0203);  // AH = 02, AL = 03
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, true}});

  // Test case 2: AL = 0x63 (99 decimal), base = 10
  // Should result in AH = 9, AL = 9 (99 / 10 = 9 remainder 9)
  helper->cpu_.registers[kIP] -= 2;      // Rewind IP (AAM is 2 bytes)
  helper->cpu_.registers[kAX] = 0x0063;  // AH = 00, AL = 63

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0909);  // AH = 09, AL = 09
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, true}});
}

TEST_F(BcdTest, AAM_EdgeCases) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-aam-edge-cases", "aam 0ah\n");

  // Test case 1: AL = 0x00, base = 10
  // Should result in AH = 0, AL = 0 (0 / 10 = 0 remainder 0)
  helper->cpu_.registers[kAX] = 0xFF00;  // AH = FF, AL = 00

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0000);  // AH = 00, AL = 00
  helper->CheckFlags({{kZF, true}, {kSF, false}, {kPF, true}});

  // Test case 2: AL = 0x09, base = 10
  // Should result in AH = 0, AL = 9 (9 / 10 = 0 remainder 9)
  helper->cpu_.registers[kIP] -= 2;      // Rewind IP
  helper->cpu_.registers[kAX] = 0xAA09;  // AH = AA, AL = 09

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0009);  // AH = 00, AL = 09
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, true}});

  // Test case 3: AL = 0x0A (10 decimal), base = 10
  // Should result in AH = 1, AL = 0 (10 / 10 = 1 remainder 0)
  helper->cpu_.registers[kIP] -= 2;      // Rewind IP
  helper->cpu_.registers[kAX] = 0x550A;  // AH = 55, AL = 0A

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0100);  // AH = 01, AL = 00
  helper->CheckFlags({{kZF, true}, {kSF, false}, {kPF, true}});
}

TEST_F(BcdTest, AAM_DifferentBases) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "test-aam-different-bases",
      "aam 02h\n"    // Base 2
      "aam 08h\n"    // Base 8
      "aam 10h\n");  // Base 16

  // Test case 1: AL = 0x07 (7 decimal), base = 2
  // Should result in AH = 3, AL = 1 (7 / 2 = 3 remainder 1)
  helper->cpu_.registers[kAX] = 0x0007;  // AH = 00, AL = 07

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0301);  // AH = 03, AL = 01
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, false}});

  // Test case 2: AL = 0x1F (31 decimal), base = 8
  // Should result in AH = 3, AL = 7 (31 / 8 = 3 remainder 7)
  helper->cpu_.registers[kAX] = 0x001F;  // AH = 00, AL = 1F

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0307);  // AH = 03, AL = 07
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, false}});

  // Test case 3: AL = 0x23 (35 decimal), base = 16
  // Should result in AH = 2, AL = 3 (35 / 16 = 2 remainder 3)
  helper->cpu_.registers[kAX] = 0x0023;  // AH = 00, AL = 23

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0203);  // AH = 02, AL = 03
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, true}});
}

TEST_F(BcdTest, AAM_MaximumValues) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-aam-maximum-values", "aam 0ah\n");

  // Test case 1: AL = 0xFF (255 decimal), base = 10
  // Should result in AH = 25, AL = 5 (255 / 10 = 25 remainder 5)
  helper->cpu_.registers[kAX] = 0x00FF;  // AH = 00, AL = FF

  helper->ExecuteInstructions(1);

  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x1905);  // AH = 19 (25 decimal), AL = 05
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, true}});

  // Test case 2: AL = 0xFE (254 decimal), base = 0xFF (255 decimal)
  helper =
      CPUTestHelper::CreateWithProgram("test-aam-maximum-base", "aam 0ffh\n");

  helper->cpu_.registers[kAX] = 0x00FE;  // AH = 00, AL = FE

  helper->ExecuteInstructions(1);

  EXPECT_EQ(
      helper->cpu_.registers[kAX],
      0x00FE);  // AH = 00, AL = FE (254 / 255 = 0 remainder 254)
  helper->CheckFlags({{kZF, false}, {kSF, true}, {kPF, false}});
}

TEST_F(BcdTest, AAM_SignFlag) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-aam-sign-flag", "aam 0ah\n");

  // Test case: AL = 0x8A (138 decimal), base = 10
  // Should result in AH = 13, AL = 8 (138 / 10 = 13 remainder 8)
  // AL = 8, which has bit 7 clear, so SF should be false
  helper->cpu_.registers[kAX] = 0x008A;  // AH = 00, AL = 8A

  helper->ExecuteInstructions(1);

  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x0D08);  // AH = 0D (13 decimal), AL = 08
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, false}});

  // Test case 2: Result with AL having bit 7 set
  // AL = 0x96 (150 decimal), base = 10
  // Should result in AH = 15, AL = 0 (150 / 10 = 15 remainder 0)
  helper->cpu_.registers[kIP] -= 2;      // Rewind IP
  helper->cpu_.registers[kAX] = 0x0096;  // AH = 00, AL = 96

  helper->ExecuteInstructions(1);

  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x0F00);  // AH = 0F (15 decimal), AL = 00
  helper->CheckFlags({{kZF, true}, {kSF, false}, {kPF, true}});
}

TEST_F(BcdTest, AAM_ParityFlag) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-aam-parity-flag", "aam 0ah\n");

  // Test case 1: Result with even parity (AL = 3, has 2 bits set)
  // AL = 0x17 (23 decimal), base = 10 -> AH = 2, AL = 3
  helper->cpu_.registers[kAX] = 0x0017;  // AH = 00, AL = 17

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0203);  // AH = 02, AL = 03
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, true}});

  // Test case 2: Result with odd parity (AL = 1, has 1 bit set)
  // AL = 0x0B (11 decimal), base = 10 -> AH = 1, AL = 1
  helper->cpu_.registers[kIP] -= 2;      // Rewind IP
  helper->cpu_.registers[kAX] = 0x000B;  // AH = 00, AL = 0B

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0101);  // AH = 01, AL = 01
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, false}});
}

TEST_F(BcdTest, AAM_TypicalBCDUsage) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "test-aam-bcd-usage",
      "mul bl\n"     // Multiply AL by BL
      "aam 0ah\n");  // Convert result to BCD

  // Simulate multiplying two BCD digits: 7 * 8 = 56
  // MUL BL will put result in AX, then AAM converts to BCD
  helper->cpu_.registers[kAX] = 0x0007;  // AL = 07
  helper->cpu_.registers[kBX] = 0x0008;  // BL = 08
  SetFlag(&helper->cpu_, kCF, false);
  SetFlag(&helper->cpu_, kOF, false);

  // Execute MUL BL
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x0038);  // AL should be 38 (7*8=56=0x38)

  // Execute AAM
  helper->ExecuteInstructions(1);

  // Expect AH = 5, AL = 6 (56 / 10 = 5 remainder 6)
  // This represents BCD result 56 (5 in AH, 6 in AL)
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0506);
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, true}});
}

TEST_F(BcdTest, AAM_BaseOne) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-aam-base-one", "aam 01h\n");

  // Test case: AL = 0x42 (66 decimal), base = 1
  // Should result in AH = 66, AL = 0 (66 / 10 = 1 remainder 0)
  helper->cpu_.registers[kAX] = 0x0042;  // AH = 00, AL = 42

  helper->ExecuteInstructions(1);

  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x4200);  // AH = 42 (66 decimal), AL = 00
  helper->CheckFlags({{kZF, true}, {kSF, false}, {kPF, true}});
}

TEST_F(BcdTest, AAM_PreservesOtherRegisters) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "test-aam-preserves-registers", "aam 0ah\n");

  // Set up other registers to verify they're not affected
  helper->cpu_.registers[kBX] = 0x1234;
  helper->cpu_.registers[kCX] = 0x5678;
  helper->cpu_.registers[kDX] = 0x9ABC;
  helper->cpu_.registers[kSP] = 0xDEF0;
  helper->cpu_.registers[kBP] = 0x1357;
  helper->cpu_.registers[kSI] = 0x2468;
  helper->cpu_.registers[kDI] = 0x9753;

  helper->cpu_.registers[kAX] = 0xFF47;  // AH = FF, AL = 47 (71 decimal)

  helper->ExecuteInstructions(1);

  // Check that AAM worked correctly: 71 / 10 = 7 remainder 1
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0701);  // AH = 07, AL = 01

  // Check that other registers are preserved
  EXPECT_EQ(helper->cpu_.registers[kBX], 0x1234);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0x5678);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x9ABC);
  EXPECT_EQ(helper->cpu_.registers[kSP], 0xDEF0);
  EXPECT_EQ(helper->cpu_.registers[kBP], 0x1357);
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x2468);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x9753);

  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, false}});
}

TEST_F(BcdTest, AAD_StandardDecimalBase) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "test-aad-standard-decimal", "aad 0ah\n");

  // Test case 1: AH = 5, AL = 6, base = 10
  // Should result in AL = 6 + 5 * 10 = 56, AH = 0
  helper->cpu_.registers[kAX] = 0x0506;  // AH = 05, AL = 06
  SetFlag(&helper->cpu_, kCF, true);  // Set some flags to test they're changed
  SetFlag(&helper->cpu_, kOF, true);

  helper->ExecuteInstructions(1);

  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x0038);  // AH = 00, AL = 38 (56 decimal)
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, false}});

  // Test case 2: AH = 9, AL = 9, base = 10
  // Should result in AL = 9 + 9 * 10 = 99, AH = 0
  helper->cpu_.registers[kIP] -= 2;      // Rewind IP (AAD is 2 bytes)
  helper->cpu_.registers[kAX] = 0x0909;  // AH = 09, AL = 09

  helper->ExecuteInstructions(1);

  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x0063);  // AH = 00, AL = 63 (99 decimal)
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, true}});
}

TEST_F(BcdTest, AAD_EdgeCases) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-aad-edge-cases", "aad 0ah\n");

  // Test case 1: AH = 0, AL = 5, base = 10
  // Should result in AL = 5 + 0 * 10 = 5, AH = 0
  helper->cpu_.registers[kAX] = 0x0005;  // AH = 00, AL = 05

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0005);  // AH = 00, AL = 05
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, true}});

  // Test case 2: AH = 3, AL = 0, base = 10
  // Should result in AL = 0 + 3 * 10 = 30, AH = 0
  helper->cpu_.registers[kIP] -= 2;      // Rewind IP
  helper->cpu_.registers[kAX] = 0x0300;  // AH = 03, AL = 00

  helper->ExecuteInstructions(1);

  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x001E);  // AH = 00, AL = 1E (30 decimal)
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, true}});

  // Test case 3: AH = 0, AL = 0, base = 10
  // Should result in AL = 0 + 0 * 10 = 0, AH = 0
  helper->cpu_.registers[kIP] -= 2;      // Rewind IP
  helper->cpu_.registers[kAX] = 0x0000;  // AH = 00, AL = 00

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0000);  // AH = 00, AL = 00
  helper->CheckFlags({{kZF, true}, {kSF, false}, {kPF, true}});
}

TEST_F(BcdTest, AAD_DifferentBases) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "test-aad-different-bases",
      "aad 02h\n"    // Base 2
      "aad 08h\n"    // Base 8
      "aad 10h\n");  // Base 16

  // Test case 1: AH = 3, AL = 1, base = 2
  // Should result in AL = 1 + 3 * 2 = 7, AH = 0
  helper->cpu_.registers[kAX] = 0x0301;  // AH = 03, AL = 01

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0007);  // AH = 00, AL = 07
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, false}});

  // Test case 2: AH = 3, AL = 7, base = 8
  // Should result in AL = 7 + 3 * 8 = 31, AH = 0
  helper->cpu_.registers[kAX] = 0x0307;  // AH = 03, AL = 07

  helper->ExecuteInstructions(1);

  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x001F);  // AH = 00, AL = 1F (31 decimal)
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, false}});

  // Test case 3: AH = 2, AL = 3, base = 16
  // Should result in AL = 3 + 2 * 16 = 35, AH = 0
  helper->cpu_.registers[kAX] = 0x0203;  // AH = 02, AL = 03

  helper->ExecuteInstructions(1);

  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x0023);  // AH = 00, AL = 23 (35 decimal)
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, false}});
}

TEST_F(BcdTest, AAD_MaximumValues) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-aad-maximum-values", "aad 0ah\n");

  // Test case 1: AH = 25, AL = 5, base = 10 (representing 255)
  // Should result in AL = 5 + 25 * 10 = 255, AH = 0
  helper->cpu_.registers[kAX] = 0x1905;  // AH = 19 (25 decimal), AL = 05

  helper->ExecuteInstructions(1);

  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x00FF);  // AH = 00, AL = FF (255 decimal)
  helper->CheckFlags({{kZF, false}, {kSF, true}, {kPF, true}});

  // Test case 2: AL = 0xFE (254 decimal), base = 0xFF (255 decimal)
  helper =
      CPUTestHelper::CreateWithProgram("test-aad-maximum-base", "aad 0ffh\n");

  helper->cpu_.registers[kAX] = 0x00FE;  // AH = 00, AL = FE

  helper->ExecuteInstructions(1);

  EXPECT_EQ(
      helper->cpu_.registers[kAX],
      0x00FE);  // AH = 00, AL = FE (254 / 255 = 0 remainder 254)
  helper->CheckFlags({{kZF, false}, {kSF, true}, {kPF, false}});
}

TEST_F(BcdTest, AAD_SignFlag) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-aad-sign-flag", "aad 0ah\n");

  // Test case 1: Result with AL having bit 7 set
  // AH = 13, AL = 8, base = 10
  // Should result in AL = 8 + 13 * 10 = 138, AH = 0
  helper->cpu_.registers[kAX] = 0x0D08;  // AH = 0D (13 decimal), AL = 08

  helper->ExecuteInstructions(1);

  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x008A);  // AH = 00, AL = 8A (138 decimal)
  helper->CheckFlags({{kZF, false}, {kSF, true}, {kPF, false}});

  // Test case 2: Result with AL having bit 7 clear
  // AH = 7, AL = 0, base = 10
  // Should result in AL = 0 + 7 * 10 = 70, AH = 0
  helper->cpu_.registers[kIP] -= 2;      // Rewind IP
  helper->cpu_.registers[kAX] = 0x0700;  // AH = 07, AL = 00

  helper->ExecuteInstructions(1);

  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x0046);  // AH = 00, AL = 46 (70 decimal)
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, false}});
}

TEST_F(BcdTest, AAD_ParityFlag) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-aad-parity-flag", "aad 0ah\n");

  // Test case 1: Result with even parity (AL = 3, has 2 bits set)
  // AH = 0, AL = 3, base = 10
  helper->cpu_.registers[kAX] = 0x0003;  // AH = 00, AL = 03

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0003);  // AH = 00, AL = 03
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, true}});

  // Test case 2: Result with odd parity (AL = 1, has 1 bit set)
  // AH = 0, AL = 1, base = 10
  helper->cpu_.registers[kIP] -= 2;      // Rewind IP
  helper->cpu_.registers[kAX] = 0x0001;  // AH = 00, AL = 01

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0001);  // AH = 00, AL = 01
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, false}});
}

TEST_F(BcdTest, AAD_TypicalBCDUsage) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "test-aad-bcd-usage",
      "aad 0ah\n"   // Convert BCD to binary
      "div bl\n");  // Divide by divisor

  // Simulate converting BCD 56 to binary then dividing by 7
  // AH = 5, AL = 6 (representing BCD 56)
  helper->cpu_.registers[kAX] = 0x0506;  // AH = 05, AL = 06 (BCD 56)
  helper->cpu_.registers[kBX] = 0x0007;  // BL = 07 (divisor)
  SetFlag(&helper->cpu_, kCF, false);
  SetFlag(&helper->cpu_, kOF, false);

  // Execute AAD - converts BCD 56 to binary 56
  helper->ExecuteInstructions(1);
  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x0038);  // AL should be 38 (56 decimal)

  // Execute DIV BL - 56 / 7 = 8 remainder 0
  helper->ExecuteInstructions(1);

  // Expect AL = 8 (quotient), AH = 0 (remainder)
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0008);
}

TEST_F(BcdTest, AAD_BaseZero) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-aad-base-zero", "aad 00h\n");

  // Test case: AH = 5, AL = 6, base = 0
  // Should result in AL = 6 + 5 * 0 = 6, AH = 0
  helper->cpu_.registers[kAX] = 0x0506;  // AH = 05, AL = 06

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0006);  // AH = 00, AL = 06
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, true}});
}

TEST_F(BcdTest, AAD_BaseOne) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-aad-base-one", "aad 01h\n");

  // Test case: AH = 42, AL = 24, base = 1
  // Should result in AL = 24 + 42 * 1 = 66, AH = 0
  helper->cpu_.registers[kAX] =
      0x2A18;  // AH = 2A (42 decimal), AL = 18 (24 decimal)

  helper->ExecuteInstructions(1);

  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x0042);  // AH = 00, AL = 42 (66 decimal)
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, true}});
}

TEST_F(BcdTest, AAD_PreservesOtherRegisters) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "test-aad-preserves-registers", "aad 0ah\n");

  // Set up other registers to verify they're not affected
  helper->cpu_.registers[kBX] = 0x1234;
  helper->cpu_.registers[kCX] = 0x5678;
  helper->cpu_.registers[kDX] = 0x9ABC;
  helper->cpu_.registers[kSP] = 0xDEF0;
  helper->cpu_.registers[kBP] = 0x1357;
  helper->cpu_.registers[kSI] = 0x2468;
  helper->cpu_.registers[kDI] = 0x9753;

  helper->cpu_.registers[kAX] = 0x0704;  // AH = 07, AL = 04

  helper->ExecuteInstructions(1);

  // Check that AAD worked correctly: 4 + 7 * 10 = 74
  EXPECT_EQ(
      helper->cpu_.registers[kAX], 0x004A);  // AH = 00, AL = 4A (74 decimal)

  // Check that other registers are preserved
  EXPECT_EQ(helper->cpu_.registers[kBX], 0x1234);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0x5678);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x9ABC);
  EXPECT_EQ(helper->cpu_.registers[kSP], 0xDEF0);
  EXPECT_EQ(helper->cpu_.registers[kBP], 0x1357);
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x2468);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x9753);

  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, false}});
}

TEST_F(BcdTest, AAD_Overflow) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-aad-overflow", "aad 0ah\n");

  // Test case that causes 8-bit overflow: AH = 30, AL = 0, base = 10
  // Should result in AL = 0 + 30 * 10 = 300 (wraps to 44), AH = 0
  helper->cpu_.registers[kAX] = 0x1E00;  // AH = 1E (30 decimal), AL = 00

  helper->ExecuteInstructions(1);

  EXPECT_EQ(
      helper->cpu_.registers[kAX],
      0x002C);  // AH = 00, AL = 2C (300 & 0xFF = 44)
  helper->CheckFlags({{kZF, false}, {kSF, false}, {kPF, false}});
}

// ============================================================================
// DAS (Decimal Adjust for Subtraction) Tests
// ============================================================================

TEST_F(BcdTest, DAS_NoAdjustmentNeeded) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-das-no-adjustment", "das\n");

  // Test case 1: AL = 42, AF = 0, CF = 0
  // No adjustment needed since low nibble = 2 <= 9 and high nibble = 4 <= 9
  helper->cpu_.registers[kAX] = 0x0042;  // AH = 00, AL = 42
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0042);  // AL unchanged
  helper->CheckFlags({{kAF, false}, {kCF, false}, {kZF, false}, {kSF, false}});

  // Test case 2: AL = 09, AF = 0, CF = 0
  // No adjustment needed (low nibble = 9, at boundary)
  auto helper2 =
      CPUTestHelper::CreateWithProgram("test-das-no-adjustment-2", "das\n");
  helper2->cpu_.registers[kAX] = 0x0009;  // AH = 00, AL = 09
  SetFlag(&helper2->cpu_, kAF, false);
  SetFlag(&helper2->cpu_, kCF, false);

  helper2->ExecuteInstructions(1);

  EXPECT_EQ(helper2->cpu_.registers[kAX], 0x0009);  // AL unchanged
  helper2->CheckFlags({{kAF, false}, {kCF, false}, {kZF, false}, {kSF, false}});

  // Test case 3: AL = 90, AF = 0, CF = 0
  // No adjustment needed (high nibble = 9, at boundary)
  auto helper3 =
      CPUTestHelper::CreateWithProgram("test-das-no-adjustment-3", "das\n");
  helper3->cpu_.registers[kAX] = 0x0090;  // AH = 00, AL = 90
  SetFlag(&helper3->cpu_, kAF, false);
  SetFlag(&helper3->cpu_, kCF, false);

  helper3->ExecuteInstructions(1);

  EXPECT_EQ(helper3->cpu_.registers[kAX], 0x0090);  // AL unchanged
  helper3->CheckFlags({{kAF, false}, {kCF, false}, {kZF, false}, {kSF, true}});
}

TEST_F(BcdTest, DAS_LowNibbleAdjustment) {
  // Test case 1: AL = 4F (low nibble = F > 9), AF = 0, CF = 0
  auto helper1 =
      CPUTestHelper::CreateWithProgram("test-das-low-nibble-1", "das\n");

  helper1->cpu_.registers[kAX] = 0x004F;  // AH = 00, AL = 4F
  SetFlag(&helper1->cpu_, kAF, false);
  SetFlag(&helper1->cpu_, kCF, false);

  helper1->ExecuteInstructions(1);

  EXPECT_EQ(helper1->cpu_.registers[kAX], 0x0049);  // AL = 4F - 6 = 49
  helper1->CheckFlags({{kAF, true}, {kCF, false}, {kZF, false}, {kSF, false}});

  // Test case 2: AL = 33, AF = 1, CF = 0
  // Low nibble adjustment due to AF being set
  auto helper2 =
      CPUTestHelper::CreateWithProgram("test-das-low-nibble-2", "das\n");

  helper2->cpu_.registers[kAX] = 0x0033;  // AH = 00, AL = 33
  SetFlag(&helper2->cpu_, kAF, true);
  SetFlag(&helper2->cpu_, kCF, false);

  helper2->ExecuteInstructions(1);

  EXPECT_EQ(helper2->cpu_.registers[kAX], 0x002D);  // AL = 33 - 6 = 2D
  helper2->CheckFlags({{kAF, true}, {kCF, false}, {kZF, false}, {kSF, false}});

  // Test case 3: AL = 0A, AF = 0, CF = 0
  // Low nibble adjustment causes underflow
  auto helper3 =
      CPUTestHelper::CreateWithProgram("test-das-low-nibble-3", "das\n");

  helper3->cpu_.registers[kAX] = 0x000A;  // AH = 00, AL = 0A
  SetFlag(&helper3->cpu_, kAF, false);
  SetFlag(&helper3->cpu_, kCF, false);

  helper3->ExecuteInstructions(1);

  EXPECT_EQ(helper3->cpu_.registers[kAX], 0x0004);  // AL = 0A - 6 = 04
  helper3->CheckFlags({{kAF, true}, {kCF, false}, {kZF, false}, {kSF, false}});
}

TEST_F(BcdTest, DAS_HighNibbleAdjustment) {
  // Test case 1: AL = A2 (high nibble = A > 9), AF = 0, CF = 0
  auto helper1 =
      CPUTestHelper::CreateWithProgram("test-das-high-nibble-1", "das\n");

  helper1->cpu_.registers[kAX] = 0x00A2;  // AH = 00, AL = A2
  SetFlag(&helper1->cpu_, kAF, false);
  SetFlag(&helper1->cpu_, kCF, false);

  helper1->ExecuteInstructions(1);

  EXPECT_EQ(helper1->cpu_.registers[kAX], 0x0042);  // AL = A2 - 60 = 42
  helper1->CheckFlags({{kAF, false}, {kCF, true}, {kZF, false}, {kSF, false}});

  // Test case 2: AL = 25, AF = 0, CF = 1
  // High nibble adjustment due to CF being set
  auto helper2 =
      CPUTestHelper::CreateWithProgram("test-das-high-nibble-2", "das\n");

  helper2->cpu_.registers[kAX] = 0x0025;  // AH = 00, AL = 25
  SetFlag(&helper2->cpu_, kAF, false);
  SetFlag(&helper2->cpu_, kCF, true);

  helper2->ExecuteInstructions(1);

  EXPECT_EQ(
      helper2->cpu_.registers[kAX], 0x00C5);  // AL = 25 - 60 = C5 (underflow)
  helper2->CheckFlags({{kAF, false}, {kCF, true}, {kZF, false}, {kSF, true}});

  // Test case 3: AL = F0, AF = 0, CF = 0
  // High nibble > 9 causes underflow with carry
  auto helper3 =
      CPUTestHelper::CreateWithProgram("test-das-high-nibble-3", "das\n");

  helper3->cpu_.registers[kAX] = 0x00F0;  // AH = 00, AL = F0
  SetFlag(&helper3->cpu_, kAF, false);
  SetFlag(&helper3->cpu_, kCF, false);

  helper3->ExecuteInstructions(1);

  EXPECT_EQ(helper3->cpu_.registers[kAX], 0x0090);  // AL = F0 - 60 = 90
  helper3->CheckFlags({{kAF, false}, {kCF, true}, {kZF, false}, {kSF, true}});
}

TEST_F(BcdTest, DAS_BothNibblesAdjustment) {
  // Test case 1: AL = AB (both nibbles > 9), AF = 0, CF = 0
  auto helper1 =
      CPUTestHelper::CreateWithProgram("test-das-both-nibbles-1", "das\n");

  helper1->cpu_.registers[kAX] = 0x00AB;  // AH = 00, AL = AB
  SetFlag(&helper1->cpu_, kAF, false);
  SetFlag(&helper1->cpu_, kCF, false);

  helper1->ExecuteInstructions(1);

  EXPECT_EQ(helper1->cpu_.registers[kAX], 0x0045);  // AL = AB - 6 - 60 = 45
  helper1->CheckFlags({{kAF, true}, {kCF, true}, {kZF, false}, {kSF, false}});

  // Test case 2: AL = FF (both nibbles = F > 9), AF = 0, CF = 0
  auto helper2 =
      CPUTestHelper::CreateWithProgram("test-das-both-nibbles-2", "das\n");

  helper2->cpu_.registers[kAX] = 0x00FF;  // AH = 00, AL = FF
  SetFlag(&helper2->cpu_, kAF, false);
  SetFlag(&helper2->cpu_, kCF, false);

  helper2->ExecuteInstructions(1);

  EXPECT_EQ(helper2->cpu_.registers[kAX], 0x0099);  // AL = FF - 6 - 60 = 99
  helper2->CheckFlags({{kAF, true}, {kCF, true}, {kZF, false}, {kSF, true}});

  // Test case 3: AL = 43, AF = 1, CF = 1
  // Both flags set even though nibbles <= 9
  auto helper3 =
      CPUTestHelper::CreateWithProgram("test-das-both-nibbles-3", "das\n");

  helper3->cpu_.registers[kAX] = 0x0043;  // AH = 00, AL = 43
  SetFlag(&helper3->cpu_, kAF, true);
  SetFlag(&helper3->cpu_, kCF, true);

  helper3->ExecuteInstructions(1);

  EXPECT_EQ(
      helper3->cpu_.registers[kAX],
      0x00DD);  // AL = 43 - 6 - 60 = DD (underflow)
  helper3->CheckFlags({{kAF, true}, {kCF, true}, {kZF, false}, {kSF, true}});
}

TEST_F(BcdTest, DAS_TypicalBCDUsage) {
  // Test case 1: Subtracting two BCD digits: 42 - 17 = 25
  auto helper1 = CPUTestHelper::CreateWithProgram(
      "test-das-bcd-usage-1",
      "sub al, bl\n"
      "das\n");

  helper1->cpu_.registers[kAX] = 0x0042;  // AL = 42 (BCD)
  helper1->cpu_.registers[kBX] = 0x0017;  // BL = 17 (BCD)
  SetFlag(&helper1->cpu_, kAF, false);
  SetFlag(&helper1->cpu_, kCF, false);

  // Execute SUB AL, BL
  helper1->ExecuteInstructions(1);
  EXPECT_EQ(
      helper1->cpu_.registers[kAX] & 0xFF,
      0x2B);  // AL should be 2B (42-17 binary)

  // Execute DAS
  helper1->ExecuteInstructions(1);

  EXPECT_EQ(
      helper1->cpu_.registers[kAX], 0x0025);  // AL = 25 (correct BCD result)
  helper1->CheckFlags({{kAF, true}, {kCF, false}, {kZF, false}, {kSF, false}});

  // Test case 2: BCD subtraction with borrow: 25 - 37 = -12 (should be 88 with
  // borrow)
  auto helper2 = CPUTestHelper::CreateWithProgram(
      "test-das-bcd-usage-2",
      "sub al, bl\n"
      "das\n");

  helper2->cpu_.registers[kAX] = 0x0025;  // AL = 25 (BCD)
  helper2->cpu_.registers[kBX] = 0x0037;  // BL = 37 (BCD)
  SetFlag(&helper2->cpu_, kAF, false);
  SetFlag(&helper2->cpu_, kCF, false);

  // Execute SUB AL, BL
  helper2->ExecuteInstructions(1);
  EXPECT_EQ(
      helper2->cpu_.registers[kAX] & 0xFF,
      0xEE);  // AL should be EE (25-37 binary with underflow)

  // Execute DAS
  helper2->ExecuteInstructions(1);

  EXPECT_EQ(helper2->cpu_.registers[kAX], 0x0088);  // AL = 88 (BCD equivalent)
  helper2->CheckFlags({{kAF, true}, {kCF, true}, {kZF, false}, {kSF, true}});

  // Test case 3: Simple BCD subtraction: 99 - 01 = 98
  auto helper3 = CPUTestHelper::CreateWithProgram(
      "test-das-bcd-usage-3",
      "sub al, bl\n"
      "das\n");

  helper3->cpu_.registers[kAX] = 0x0099;  // AL = 99 (BCD)
  helper3->cpu_.registers[kBX] = 0x0001;  // BL = 01 (BCD)
  SetFlag(&helper3->cpu_, kAF, false);
  SetFlag(&helper3->cpu_, kCF, false);

  // Execute SUB AL, BL
  helper3->ExecuteInstructions(1);
  EXPECT_EQ(
      helper3->cpu_.registers[kAX] & 0xFF,
      0x98);  // AL should be 98 (99-01 binary)

  // Execute DAS
  helper3->ExecuteInstructions(1);

  EXPECT_EQ(
      helper3->cpu_.registers[kAX], 0x0098);  // AL = 98 (no adjustment needed)
  helper3->CheckFlags({{kAF, false}, {kCF, false}, {kZF, false}, {kSF, true}});
}

TEST_F(BcdTest, DAS_EdgeCases) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-das-edge-cases", "das\n");

  // Test case 1: AL = 00, check zero flag
  helper->cpu_.registers[kAX] = 0x0000;  // AH = 00, AL = 00
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0000);  // AL unchanged
  helper->CheckFlags({{kAF, false}, {kCF, false}, {kZF, true}, {kSF, false}});

  // Test case 2: AL = 06, AF = 1 (underflow case)
  auto helper2 =
      CPUTestHelper::CreateWithProgram("test-das-edge-cases-2", "das\n");
  helper2->cpu_.registers[kAX] = 0x0006;  // AH = 00, AL = 06
  SetFlag(&helper2->cpu_, kAF, true);
  SetFlag(&helper2->cpu_, kCF, false);

  helper2->ExecuteInstructions(1);

  EXPECT_EQ(helper2->cpu_.registers[kAX], 0x0000);  // AL = 06 - 6 = 00
  helper2->CheckFlags({{kAF, true}, {kCF, false}, {kZF, true}, {kSF, false}});

  // Test case 3: AL = 60, CF = 1 (high nibble underflow)
  auto helper3 =
      CPUTestHelper::CreateWithProgram("test-das-edge-cases-3", "das\n");
  helper3->cpu_.registers[kAX] = 0x0060;  // AH = 00, AL = 60
  SetFlag(&helper3->cpu_, kAF, false);
  SetFlag(&helper3->cpu_, kCF, true);

  helper3->ExecuteInstructions(1);

  EXPECT_EQ(helper3->cpu_.registers[kAX], 0x0000);  // AL = 60 - 60 = 00
  helper3->CheckFlags({{kAF, false}, {kCF, true}, {kZF, true}, {kSF, false}});
}

TEST_F(BcdTest, DAS_PreservesOtherRegisters) {
  auto helper =
      CPUTestHelper::CreateWithProgram("test-das-preserves-registers", "das\n");

  // Set up other registers to verify they're not affected
  helper->cpu_.registers[kBX] = 0x1234;
  helper->cpu_.registers[kCX] = 0x5678;
  helper->cpu_.registers[kDX] = 0x9ABC;
  helper->cpu_.registers[kSP] = 0xDEF0;
  helper->cpu_.registers[kBP] = 0x1357;
  helper->cpu_.registers[kSI] = 0x2468;
  helper->cpu_.registers[kDI] = 0x9753;

  helper->cpu_.registers[kAX] = 0x0ABC;  // AH = 0A, AL = BC (will be adjusted)
  SetFlag(&helper->cpu_, kAF, false);
  SetFlag(&helper->cpu_, kCF, false);

  helper->ExecuteInstructions(1);

  // Check that DAS worked on AL: BC - 6 - 60 = 56
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x0A56);  // AH preserved, AL adjusted

  // Check that other registers are preserved
  EXPECT_EQ(helper->cpu_.registers[kBX], 0x1234);
  EXPECT_EQ(helper->cpu_.registers[kCX], 0x5678);
  EXPECT_EQ(helper->cpu_.registers[kDX], 0x9ABC);
  EXPECT_EQ(helper->cpu_.registers[kSP], 0xDEF0);
  EXPECT_EQ(helper->cpu_.registers[kBP], 0x1357);
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x2468);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x9753);

  helper->CheckFlags({{kAF, true}, {kCF, true}, {kZF, false}, {kSF, false}});
}
