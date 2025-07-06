#include <gtest/gtest.h>

#include "./test_helpers.h"
#include "cpu.h"

class StringRepCmpTest : public ::testing::Test {};

TEST_F(StringRepCmpTest, SCASBBasic) {
  // Test basic SCASB functionality - AL equals memory
  auto helper = CPUTestHelper::CreateWithProgram("scasb-basic-test", "scasb\n");
  helper->cpu_.registers[kES] = 0x030;
  helper->cpu_.registers[kDI] = 0x00;
  helper->cpu_.registers[kAX] =
      (helper->cpu_.registers[kAX] & 0xFF00) | 0x55;  // Set AL = 0x55

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up memory value to compare
  helper->memory_[0x300] = 0x55;

  // Execute SCASB
  helper->ExecuteInstructions(1);

  // Check flags - equal comparison should set ZF
  EXPECT_TRUE(GetFlag(&helper->cpu_, kZF));
  EXPECT_FALSE(GetFlag(&helper->cpu_, kCF));

  // DI should increment by 1
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x01);
}

TEST_F(StringRepCmpTest, SCASBNotEqual) {
  // Test SCASB with non-equal values
  auto helper =
      CPUTestHelper::CreateWithProgram("scasb-not-equal-test", "scasb\n");
  helper->cpu_.registers[kES] = 0x030;
  helper->cpu_.registers[kDI] = 0x00;
  helper->cpu_.registers[kAX] =
      (helper->cpu_.registers[kAX] & 0xFF00) | 0x33;  // Set AL = 0x33

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up memory value
  helper->memory_[0x300] = 0x55;

  // Execute SCASB
  helper->ExecuteInstructions(1);

  // Check flags - unequal comparison, 0x33 < 0x55
  EXPECT_FALSE(GetFlag(&helper->cpu_, kZF));
  EXPECT_TRUE(GetFlag(&helper->cpu_, kCF));  // Carry flag set when AL < memory

  // DI should increment by 1
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x01);
}

TEST_F(StringRepCmpTest, SCASWBasic) {
  // Test basic SCASW functionality - AX equals memory
  auto helper = CPUTestHelper::CreateWithProgram("scasw-basic-test", "scasw\n");
  helper->cpu_.registers[kES] = 0x040;
  helper->cpu_.registers[kDI] = 0x00;
  helper->cpu_.registers[kAX] = 0x1234;

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up memory value to compare (little endian)
  helper->memory_[0x400] = 0x34;
  helper->memory_[0x401] = 0x12;

  // Execute SCASW
  helper->ExecuteInstructions(1);

  // Check flags - equal comparison should set ZF
  EXPECT_TRUE(GetFlag(&helper->cpu_, kZF));
  EXPECT_FALSE(GetFlag(&helper->cpu_, kCF));

  // DI should increment by 2
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x02);
}

TEST_F(StringRepCmpTest, SCASBBackward) {
  // Test SCASB with direction flag set (backward)
  auto helper =
      CPUTestHelper::CreateWithProgram("scasb-backward-test", "scasb\n");
  helper->cpu_.registers[kES] = 0x050;
  helper->cpu_.registers[kDI] = 0x00;
  helper->cpu_.registers[kAX] =
      (helper->cpu_.registers[kAX] & 0xFF00) | 0x77;  // Set AL = 0x77

  // Set direction flag (backward direction)
  SetFlag(&helper->cpu_, kDF, true);

  // Set up memory value
  helper->memory_[0x500] = 0x77;

  // Execute SCASB
  helper->ExecuteInstructions(1);

  // Check flags
  EXPECT_TRUE(GetFlag(&helper->cpu_, kZF));
  EXPECT_FALSE(GetFlag(&helper->cpu_, kCF));

  // DI should decrement by 1
  EXPECT_EQ(helper->cpu_.registers[kDI], 0xFFFF);
}

TEST_F(StringRepCmpTest, SCASWBackward) {
  // Test SCASW with direction flag set (backward)
  auto helper =
      CPUTestHelper::CreateWithProgram("scasw-backward-test", "scasw\n");
  helper->cpu_.registers[kES] = 0x060;
  helper->cpu_.registers[kDI] = 0x00;
  helper->cpu_.registers[kAX] = 0xABCD;

  // Set direction flag (backward direction)
  SetFlag(&helper->cpu_, kDF, true);

  // Set up memory value (little endian)
  helper->memory_[0x600] = 0xCD;
  helper->memory_[0x601] = 0xAB;

  // Execute SCASW
  helper->ExecuteInstructions(1);

  // Check flags
  EXPECT_TRUE(GetFlag(&helper->cpu_, kZF));
  EXPECT_FALSE(GetFlag(&helper->cpu_, kCF));

  // DI should decrement by 2
  EXPECT_EQ(helper->cpu_.registers[kDI], 0xFFFE);
}

TEST_F(StringRepCmpTest, REPEScasbFound) {
  // Test REPE SCASB - find unequal byte (stops when ZF=0)
  auto helper =
      CPUTestHelper::CreateWithProgram("repe-scasb-found-test", "repe scasb\n");
  helper->cpu_.registers[kES] = 0x070;
  helper->cpu_.registers[kDI] = 0x00;
  helper->cpu_.registers[kAX] =
      (helper->cpu_.registers[kAX] & 0xFF00) | 0xAA;  // Set AL = 0xAA
  helper->cpu_.registers[kCX] = 4;                    // Check 4 bytes

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up memory - first two bytes match, third doesn't
  helper->memory_[0x700] = 0xAA;  // First byte matches
  helper->memory_[0x701] = 0xAA;  // Second byte matches
  helper->memory_[0x702] = 0xBB;  // Third byte doesn't match - should stop here
  helper->memory_[0x703] = 0xAA;  // Fourth byte (shouldn't reach)

  // Execute REPE SCASB
  helper->ExecuteInstructions(1);

  // Should stop at third byte where comparison fails
  EXPECT_EQ(
      helper->cpu_.registers[kDI], 0x03);     // Points after the unequal byte
  EXPECT_EQ(helper->cpu_.registers[kCX], 1);  // One iteration left
  EXPECT_FALSE(
      GetFlag(&helper->cpu_, kZF));  // ZF clear because last comparison failed
}

TEST_F(StringRepCmpTest, REPEScasbNotFound) {
  // Test REPE SCASB - all bytes equal, CX reaches zero
  auto helper = CPUTestHelper::CreateWithProgram(
      "repe-scasb-not-found-test", "repe scasb\n");
  helper->cpu_.registers[kES] = 0x080;
  helper->cpu_.registers[kDI] = 0x00;
  helper->cpu_.registers[kAX] =
      (helper->cpu_.registers[kAX] & 0xFF00) | 0xCC;  // Set AL = 0xCC
  helper->cpu_.registers[kCX] = 3;                    // Check 3 bytes

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up memory - all bytes match
  helper->memory_[0x800] = 0xCC;
  helper->memory_[0x801] = 0xCC;
  helper->memory_[0x802] = 0xCC;

  // Execute REPE SCASB
  helper->ExecuteInstructions(1);

  // Should complete all iterations
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x03);  // Moved through all 3 bytes
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);     // All iterations completed
  EXPECT_TRUE(
      GetFlag(&helper->cpu_, kZF));  // ZF set because last comparison succeeded
}

TEST_F(StringRepCmpTest, REPEScaswFound) {
  // Test REPE SCASW - find unequal word
  auto helper =
      CPUTestHelper::CreateWithProgram("repe-scasw-found-test", "repe scasw\n");
  helper->cpu_.registers[kES] = 0x090;
  helper->cpu_.registers[kDI] = 0x00;
  helper->cpu_.registers[kAX] = 0x1111;
  helper->cpu_.registers[kCX] = 3;  // Check 3 words

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up memory - first word matches, second doesn't (little endian)
  helper->memory_[0x900] = 0x11;  // First word matches
  helper->memory_[0x901] = 0x11;
  helper->memory_[0x902] =
      0x22;  // Second word doesn't match - should stop here
  helper->memory_[0x903] = 0x22;
  helper->memory_[0x904] = 0x11;  // Third word (shouldn't reach)
  helper->memory_[0x905] = 0x11;

  // Execute REPE SCASW
  helper->ExecuteInstructions(1);

  // Should stop at second word where comparison fails
  EXPECT_EQ(
      helper->cpu_.registers[kDI], 0x04);     // Points after the unequal word
  EXPECT_EQ(helper->cpu_.registers[kCX], 1);  // One iteration left
  EXPECT_FALSE(
      GetFlag(&helper->cpu_, kZF));  // ZF clear because last comparison failed
}

TEST_F(StringRepCmpTest, REPNEScasbFound) {
  // Test REPNE SCASB - find equal byte (stops when ZF=1)
  auto helper = CPUTestHelper::CreateWithProgram(
      "repne-scasb-found-test", "repne scasb\n");
  helper->cpu_.registers[kES] = 0x0A0;
  helper->cpu_.registers[kDI] = 0x00;
  helper->cpu_.registers[kAX] =
      (helper->cpu_.registers[kAX] & 0xFF00) | 0xDD;  // Set AL = 0xDD
  helper->cpu_.registers[kCX] = 4;                    // Check up to 4 bytes

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up memory - first two bytes don't match, third matches
  helper->memory_[0xA00] = 0x11;  // First byte doesn't match
  helper->memory_[0xA01] = 0x22;  // Second byte doesn't match
  helper->memory_[0xA02] = 0xDD;  // Third byte matches - should stop here
  helper->memory_[0xA03] = 0x44;  // Fourth byte (shouldn't reach)

  // Execute REPNE SCASB
  helper->ExecuteInstructions(1);

  // Should stop at third byte where comparison succeeds
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x03);  // Points after the equal byte
  EXPECT_EQ(helper->cpu_.registers[kCX], 1);     // One iteration left
  EXPECT_TRUE(
      GetFlag(&helper->cpu_, kZF));  // ZF set because last comparison succeeded
}

TEST_F(StringRepCmpTest, REPNEScasbNotFound) {
  // Test REPNE SCASB - no equal byte found, CX reaches zero
  auto helper = CPUTestHelper::CreateWithProgram(
      "repne-scasb-not-found-test", "repne scasb\n");
  helper->cpu_.registers[kES] = 0x0B0;
  helper->cpu_.registers[kDI] = 0x00;
  helper->cpu_.registers[kAX] =
      (helper->cpu_.registers[kAX] & 0xFF00) | 0xFF;  // Set AL = 0xFF
  helper->cpu_.registers[kCX] = 3;                    // Check 3 bytes

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up memory - all bytes don't match
  helper->memory_[0xB00] = 0x11;
  helper->memory_[0xB01] = 0x22;
  helper->memory_[0xB02] = 0x33;

  // Execute REPNE SCASB
  helper->ExecuteInstructions(1);

  // Should complete all iterations without finding match
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x03);  // Moved through all 3 bytes
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);     // All iterations completed
  EXPECT_FALSE(
      GetFlag(&helper->cpu_, kZF));  // ZF clear because last comparison failed
}

TEST_F(StringRepCmpTest, REPNEScaswFound) {
  // Test REPNE SCASW - find equal word
  auto helper = CPUTestHelper::CreateWithProgram(
      "repne-scasw-found-test", "repne scasw\n");
  helper->cpu_.registers[kES] = 0x0C0;
  helper->cpu_.registers[kDI] = 0x00;
  helper->cpu_.registers[kAX] = 0x5555;
  helper->cpu_.registers[kCX] = 3;  // Check up to 3 words

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up memory - first word doesn't match, second matches (little endian)
  helper->memory_[0xC00] = 0x11;  // First word doesn't match
  helper->memory_[0xC01] = 0x11;
  helper->memory_[0xC02] = 0x55;  // Second word matches - should stop here
  helper->memory_[0xC03] = 0x55;
  helper->memory_[0xC04] = 0x33;  // Third word (shouldn't reach)
  helper->memory_[0xC05] = 0x33;

  // Execute REPNE SCASW
  helper->ExecuteInstructions(1);

  // Should stop at second word where comparison succeeds
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x04);  // Points after the equal word
  EXPECT_EQ(helper->cpu_.registers[kCX], 1);     // One iteration left
  EXPECT_TRUE(
      GetFlag(&helper->cpu_, kZF));  // ZF set because last comparison succeeded
}

TEST_F(StringRepCmpTest, SCASBZeroCount) {
  // Test REPE SCASB with CX = 0 (should not execute)
  auto helper =
      CPUTestHelper::CreateWithProgram("scasb-zero-count-test", "repe scasb\n");
  helper->cpu_.registers[kES] = 0x0D0;
  helper->cpu_.registers[kDI] = 0x00;
  helper->cpu_.registers[kAX] =
      (helper->cpu_.registers[kAX] & 0xFF00) | 0x99;  // Set AL = 0x99
  helper->cpu_.registers[kCX] = 0;                    // Zero count

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up memory value
  helper->memory_[0xD00] = 0x99;

  // Execute REPE SCASB
  helper->ExecuteInstructions(1);

  // Should not modify anything
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x00);  // DI unchanged
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);     // CX still zero
}

TEST_F(StringRepCmpTest, SCASBSignedComparison) {
  // Test SCASB with signed comparison behavior
  auto helper =
      CPUTestHelper::CreateWithProgram("scasb-signed-test", "scasb\n");
  helper->cpu_.registers[kES] = 0x0E0;
  helper->cpu_.registers[kDI] = 0x00;
  helper->cpu_.registers[kAX] =
      (helper->cpu_.registers[kAX] & 0xFF00) |
      0x7F;  // Set AL = 0x7F (+127 in signed interpretation)

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up memory value
  helper->memory_[0xE00] = 0x80;  // -128 in signed interpretation

  // Execute SCASB
  helper->ExecuteInstructions(1);

  // Check flags - 0x7F compared to 0x80
  EXPECT_FALSE(GetFlag(&helper->cpu_, kZF));  // Not equal
  EXPECT_TRUE(
      GetFlag(&helper->cpu_, kCF));  // 0x7F < 0x80 in unsigned comparison
  EXPECT_TRUE(GetFlag(&helper->cpu_, kSF));  // Sign flag reflects result sign
  EXPECT_TRUE(
      GetFlag(&helper->cpu_, kOF));  // Overflow: positive - negative = overflow
}

TEST_F(StringRepCmpTest, REPEScasbBackward) {
  // Test REPE SCASB in backward direction
  auto helper = CPUTestHelper::CreateWithProgram(
      "repe-scasb-backward-test", "repe scasb\n");
  helper->cpu_.registers[kES] = 0x0F0;
  helper->cpu_.registers[kDI] = 0x02;  // Start at offset 2
  helper->cpu_.registers[kAX] =
      (helper->cpu_.registers[kAX] & 0xFF00) | 0x55;  // Set AL = 0x55
  helper->cpu_.registers[kCX] = 3;                    // Check 3 bytes

  // Set direction flag (backward direction)
  SetFlag(&helper->cpu_, kDF, true);

  // Set up memory
  helper->memory_[0xF02] = 0x55;  // Third byte matches
  helper->memory_[0xF01] = 0x55;  // Second byte matches
  helper->memory_[0xF00] = 0x66;  // First byte doesn't match - should stop here

  // Execute REPE SCASB
  helper->ExecuteInstructions(1);

  // Should stop after checking byte at 0xF00
  EXPECT_EQ(
      helper->cpu_.registers[kDI], 0xFFFF);   // Points to 0xF00-1 (wrapped)
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);  // All iterations completed
  EXPECT_FALSE(
      GetFlag(&helper->cpu_, kZF));  // ZF clear because last comparison failed
}

// ============================================================================
// CMPS (Compare String) instruction tests
// ============================================================================

TEST_F(StringRepCmpTest, CMPSBBasic) {
  // Test basic CMPSB functionality - compare equal bytes
  auto helper = CPUTestHelper::CreateWithProgram("cmpsb-basic-test", "cmpsb\n");
  helper->cpu_.registers[kDS] = 0x020;
  helper->cpu_.registers[kES] = 0x030;
  helper->cpu_.registers[kSI] = 0x00;
  helper->cpu_.registers[kDI] = 0x00;

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source and destination data - equal bytes
  helper->memory_[0x200] = 0x55;
  helper->memory_[0x300] = 0x55;

  // Execute CMPSB
  helper->ExecuteInstructions(1);

  // Check flags - equal comparison should set ZF
  EXPECT_TRUE(GetFlag(&helper->cpu_, kZF));
  EXPECT_FALSE(GetFlag(&helper->cpu_, kCF));

  // SI and DI should both increment by 1
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x01);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x01);
}

TEST_F(StringRepCmpTest, CMPSBNotEqual) {
  // Test CMPSB with non-equal values
  auto helper =
      CPUTestHelper::CreateWithProgram("cmpsb-not-equal-test", "cmpsb\n");
  helper->cpu_.registers[kDS] = 0x020;
  helper->cpu_.registers[kES] = 0x030;
  helper->cpu_.registers[kSI] = 0x00;
  helper->cpu_.registers[kDI] = 0x00;

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source and destination data - source < destination
  helper->memory_[0x200] = 0x33;
  helper->memory_[0x300] = 0x55;

  // Execute CMPSB
  helper->ExecuteInstructions(1);

  // Check flags - unequal comparison, 0x33 < 0x55
  EXPECT_FALSE(GetFlag(&helper->cpu_, kZF));
  EXPECT_TRUE(
      GetFlag(&helper->cpu_, kCF));  // Carry flag set when source < dest

  // SI and DI should both increment by 1
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x01);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x01);
}

TEST_F(StringRepCmpTest, CMPSWBasic) {
  // Test basic CMPSW functionality - compare equal words
  auto helper = CPUTestHelper::CreateWithProgram("cmpsw-basic-test", "cmpsw\n");
  helper->cpu_.registers[kDS] = 0x020;
  helper->cpu_.registers[kES] = 0x030;
  helper->cpu_.registers[kSI] = 0x00;
  helper->cpu_.registers[kDI] = 0x00;

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source and destination data - equal words (little endian)
  helper->memory_[0x200] = 0x34;  // Low byte of 0x1234
  helper->memory_[0x201] = 0x12;  // High byte of 0x1234
  helper->memory_[0x300] = 0x34;  // Low byte of 0x1234
  helper->memory_[0x301] = 0x12;  // High byte of 0x1234

  // Execute CMPSW
  helper->ExecuteInstructions(1);

  // Check flags - equal comparison should set ZF
  EXPECT_TRUE(GetFlag(&helper->cpu_, kZF));
  EXPECT_FALSE(GetFlag(&helper->cpu_, kCF));

  // SI and DI should both increment by 2
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x02);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x02);
}

TEST_F(StringRepCmpTest, CMPSBBackward) {
  // Test CMPSB with direction flag set (backward)
  auto helper =
      CPUTestHelper::CreateWithProgram("cmpsb-backward-test", "cmpsb\n");
  helper->cpu_.registers[kDS] = 0x020;
  helper->cpu_.registers[kES] = 0x030;
  helper->cpu_.registers[kSI] = 0x05;
  helper->cpu_.registers[kDI] = 0x05;

  // Set direction flag (backward direction)
  SetFlag(&helper->cpu_, kDF, true);

  // Set up source and destination data
  helper->memory_[0x205] = 0x77;
  helper->memory_[0x305] = 0x77;

  // Execute CMPSB
  helper->ExecuteInstructions(1);

  // Check flags
  EXPECT_TRUE(GetFlag(&helper->cpu_, kZF));
  EXPECT_FALSE(GetFlag(&helper->cpu_, kCF));

  // SI and DI should both decrement by 1
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x04);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x04);
}

TEST_F(StringRepCmpTest, CMPSWBackward) {
  // Test CMPSW with direction flag set (backward)
  auto helper =
      CPUTestHelper::CreateWithProgram("cmpsw-backward-test", "cmpsw\n");
  helper->cpu_.registers[kDS] = 0x020;
  helper->cpu_.registers[kES] = 0x030;
  helper->cpu_.registers[kSI] = 0x05;
  helper->cpu_.registers[kDI] = 0x05;

  // Set direction flag (backward direction)
  SetFlag(&helper->cpu_, kDF, true);

  // Set up source and destination data (little endian)
  helper->memory_[0x204] = 0xCD;  // Low byte of 0xABCD
  helper->memory_[0x205] = 0xAB;  // High byte of 0xABCD
  helper->memory_[0x304] = 0xCD;  // Low byte of 0xABCD
  helper->memory_[0x305] = 0xAB;  // High byte of 0xABCD

  // Execute CMPSW
  helper->ExecuteInstructions(1);

  // Check flags
  EXPECT_TRUE(GetFlag(&helper->cpu_, kZF));
  EXPECT_FALSE(GetFlag(&helper->cpu_, kCF));

  // SI and DI should both decrement by 2
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x03);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x03);
}

TEST_F(StringRepCmpTest, REPECmpsbFound) {
  // Test REPE CMPSB - find unequal byte (stops when ZF=0)
  auto helper =
      CPUTestHelper::CreateWithProgram("repe-cmpsb-found-test", "repe cmpsb\n");
  helper->cpu_.registers[kDS] = 0x020;
  helper->cpu_.registers[kES] = 0x030;
  helper->cpu_.registers[kSI] = 0x00;
  helper->cpu_.registers[kDI] = 0x00;
  helper->cpu_.registers[kCX] = 4;  // Compare up to 4 bytes

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source and destination data - first two bytes match, third doesn't
  helper->memory_[0x200] = 0xAA;  // First byte matches
  helper->memory_[0x201] = 0xAA;  // Second byte matches
  helper->memory_[0x202] = 0xBB;  // Third byte doesn't match - should stop here
  helper->memory_[0x203] = 0xAA;  // Fourth byte (shouldn't reach)

  helper->memory_[0x300] = 0xAA;  // First byte matches
  helper->memory_[0x301] = 0xAA;  // Second byte matches
  helper->memory_[0x302] = 0xCC;  // Third byte doesn't match - should stop here
  helper->memory_[0x303] = 0xAA;  // Fourth byte (shouldn't reach)

  // Execute REPE CMPSB
  helper->ExecuteInstructions(1);

  // Should stop at third byte where comparison fails
  EXPECT_EQ(
      helper->cpu_.registers[kSI], 0x03);  // Points after the unequal byte
  EXPECT_EQ(
      helper->cpu_.registers[kDI], 0x03);     // Points after the unequal byte
  EXPECT_EQ(helper->cpu_.registers[kCX], 1);  // One iteration left
  EXPECT_FALSE(
      GetFlag(&helper->cpu_, kZF));  // ZF clear because last comparison failed
}

TEST_F(StringRepCmpTest, REPECmpsbNotFound) {
  // Test REPE CMPSB - all bytes equal, CX reaches zero
  auto helper = CPUTestHelper::CreateWithProgram(
      "repe-cmpsb-not-found-test", "repe cmpsb\n");
  helper->cpu_.registers[kDS] = 0x020;
  helper->cpu_.registers[kES] = 0x030;
  helper->cpu_.registers[kSI] = 0x00;
  helper->cpu_.registers[kDI] = 0x00;
  helper->cpu_.registers[kCX] = 3;  // Compare 3 bytes

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source and destination data - all bytes match
  helper->memory_[0x200] = 0xCC;
  helper->memory_[0x201] = 0xCC;
  helper->memory_[0x202] = 0xCC;
  helper->memory_[0x300] = 0xCC;
  helper->memory_[0x301] = 0xCC;
  helper->memory_[0x302] = 0xCC;

  // Execute REPE CMPSB
  helper->ExecuteInstructions(1);

  // Should complete all iterations
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x03);  // Moved through all 3 bytes
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x03);  // Moved through all 3 bytes
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);     // All iterations completed
  EXPECT_TRUE(
      GetFlag(&helper->cpu_, kZF));  // ZF set because last comparison succeeded
}

TEST_F(StringRepCmpTest, REPECmpswFound) {
  // Test REPE CMPSW - find unequal word
  auto helper =
      CPUTestHelper::CreateWithProgram("repe-cmpsw-found-test", "repe cmpsw\n");
  helper->cpu_.registers[kDS] = 0x020;
  helper->cpu_.registers[kES] = 0x030;
  helper->cpu_.registers[kSI] = 0x00;
  helper->cpu_.registers[kDI] = 0x00;
  helper->cpu_.registers[kCX] = 3;  // Compare up to 3 words

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source and destination data - first word matches, second doesn't
  // (little endian)
  helper->memory_[0x200] = 0x11;  // First word matches
  helper->memory_[0x201] = 0x11;
  helper->memory_[0x202] =
      0x22;  // Second word doesn't match - should stop here
  helper->memory_[0x203] = 0x22;
  helper->memory_[0x204] = 0x11;  // Third word (shouldn't reach)
  helper->memory_[0x205] = 0x11;

  helper->memory_[0x300] = 0x11;  // First word matches
  helper->memory_[0x301] = 0x11;
  helper->memory_[0x302] =
      0x33;  // Second word doesn't match - should stop here
  helper->memory_[0x303] = 0x33;
  helper->memory_[0x304] = 0x11;  // Third word (shouldn't reach)
  helper->memory_[0x305] = 0x11;

  // Execute REPE CMPSW
  helper->ExecuteInstructions(1);

  // Should stop at second word where comparison fails
  EXPECT_EQ(
      helper->cpu_.registers[kSI], 0x04);  // Points after the unequal word
  EXPECT_EQ(
      helper->cpu_.registers[kDI], 0x04);     // Points after the unequal word
  EXPECT_EQ(helper->cpu_.registers[kCX], 1);  // One iteration left
  EXPECT_FALSE(
      GetFlag(&helper->cpu_, kZF));  // ZF clear because last comparison failed
}

TEST_F(StringRepCmpTest, REPNECmpsbFound) {
  // Test REPNE CMPSB - find equal byte (stops when ZF=1)
  auto helper = CPUTestHelper::CreateWithProgram(
      "repne-cmpsb-found-test", "repne cmpsb\n");
  helper->cpu_.registers[kDS] = 0x020;
  helper->cpu_.registers[kES] = 0x030;
  helper->cpu_.registers[kSI] = 0x00;
  helper->cpu_.registers[kDI] = 0x00;
  helper->cpu_.registers[kCX] = 4;  // Compare up to 4 bytes

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source and destination data - first two bytes don't match, third
  // matches
  helper->memory_[0x200] = 0x11;  // First byte doesn't match
  helper->memory_[0x201] = 0x22;  // Second byte doesn't match
  helper->memory_[0x202] = 0xDD;  // Third byte matches - should stop here
  helper->memory_[0x203] = 0x44;  // Fourth byte (shouldn't reach)

  helper->memory_[0x300] = 0x55;  // First byte doesn't match
  helper->memory_[0x301] = 0x66;  // Second byte doesn't match
  helper->memory_[0x302] = 0xDD;  // Third byte matches - should stop here
  helper->memory_[0x303] = 0x77;  // Fourth byte (shouldn't reach)

  // Execute REPNE CMPSB
  helper->ExecuteInstructions(1);

  // Should stop at third byte where comparison succeeds
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x03);  // Points after the equal byte
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x03);  // Points after the equal byte
  EXPECT_EQ(helper->cpu_.registers[kCX], 1);     // One iteration left
  EXPECT_TRUE(
      GetFlag(&helper->cpu_, kZF));  // ZF set because last comparison succeeded
}

TEST_F(StringRepCmpTest, REPNECmpsbNotFound) {
  // Test REPNE CMPSB - no equal byte found, CX reaches zero
  auto helper = CPUTestHelper::CreateWithProgram(
      "repne-cmpsb-not-found-test", "repne cmpsb\n");
  helper->cpu_.registers[kDS] = 0x020;
  helper->cpu_.registers[kES] = 0x030;
  helper->cpu_.registers[kSI] = 0x00;
  helper->cpu_.registers[kDI] = 0x00;
  helper->cpu_.registers[kCX] = 3;  // Compare 3 bytes

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source and destination data - all bytes don't match
  helper->memory_[0x200] = 0x11;
  helper->memory_[0x201] = 0x22;
  helper->memory_[0x202] = 0x33;
  helper->memory_[0x300] = 0x55;
  helper->memory_[0x301] = 0x66;
  helper->memory_[0x302] = 0x77;

  // Execute REPNE CMPSB
  helper->ExecuteInstructions(1);

  // Should complete all iterations without finding match
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x03);  // Moved through all 3 bytes
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x03);  // Moved through all 3 bytes
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);     // All iterations completed
  EXPECT_FALSE(
      GetFlag(&helper->cpu_, kZF));  // ZF clear because last comparison failed
}

TEST_F(StringRepCmpTest, REPNECmpswFound) {
  // Test REPNE CMPSW - find equal word
  auto helper = CPUTestHelper::CreateWithProgram(
      "repne-cmpsw-found-test", "repne cmpsw\n");
  helper->cpu_.registers[kDS] = 0x020;
  helper->cpu_.registers[kES] = 0x030;
  helper->cpu_.registers[kSI] = 0x00;
  helper->cpu_.registers[kDI] = 0x00;
  helper->cpu_.registers[kCX] = 3;  // Compare up to 3 words

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source and destination data - first word doesn't match, second
  // matches (little endian)
  helper->memory_[0x200] = 0x11;  // First word doesn't match
  helper->memory_[0x201] = 0x11;
  helper->memory_[0x202] = 0x55;  // Second word matches - should stop here
  helper->memory_[0x203] = 0x55;
  helper->memory_[0x204] = 0x33;  // Third word (shouldn't reach)
  helper->memory_[0x205] = 0x33;

  helper->memory_[0x300] = 0x22;  // First word doesn't match
  helper->memory_[0x301] = 0x22;
  helper->memory_[0x302] = 0x55;  // Second word matches - should stop here
  helper->memory_[0x303] = 0x55;
  helper->memory_[0x304] = 0x44;  // Third word (shouldn't reach)
  helper->memory_[0x305] = 0x44;

  // Execute REPNE CMPSW
  helper->ExecuteInstructions(1);

  // Should stop at second word where comparison succeeds
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x04);  // Points after the equal word
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x04);  // Points after the equal word
  EXPECT_EQ(helper->cpu_.registers[kCX], 1);     // One iteration left
  EXPECT_TRUE(
      GetFlag(&helper->cpu_, kZF));  // ZF set because last comparison succeeded
}

TEST_F(StringRepCmpTest, CMPSBZeroCount) {
  // Test REPE CMPSB with CX = 0 (should not execute)
  auto helper =
      CPUTestHelper::CreateWithProgram("cmpsb-zero-count-test", "repe cmpsb\n");
  helper->cpu_.registers[kDS] = 0x020;
  helper->cpu_.registers[kES] = 0x030;
  helper->cpu_.registers[kSI] = 0x00;
  helper->cpu_.registers[kDI] = 0x00;
  helper->cpu_.registers[kCX] = 0;  // Zero count

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source and destination data
  helper->memory_[0x200] = 0x99;
  helper->memory_[0x300] = 0x99;

  // Execute REPE CMPSB
  helper->ExecuteInstructions(1);

  // Should not modify anything
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x00);  // SI unchanged
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x00);  // DI unchanged
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);     // CX still zero
}

TEST_F(StringRepCmpTest, CMPSBSignedComparison) {
  // Test CMPSB with signed comparison behavior
  auto helper =
      CPUTestHelper::CreateWithProgram("cmpsb-signed-test", "cmpsb\n");
  helper->cpu_.registers[kDS] = 0x020;
  helper->cpu_.registers[kES] = 0x030;
  helper->cpu_.registers[kSI] = 0x00;
  helper->cpu_.registers[kDI] = 0x00;

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source and destination data
  helper->memory_[0x200] = 0x7F;  // +127 in signed interpretation
  helper->memory_[0x300] = 0x80;  // -128 in signed interpretation

  // Execute CMPSB
  helper->ExecuteInstructions(1);

  // Check flags - 0x7F compared to 0x80
  EXPECT_FALSE(GetFlag(&helper->cpu_, kZF));  // Not equal
  EXPECT_TRUE(
      GetFlag(&helper->cpu_, kCF));  // 0x7F < 0x80 in unsigned comparison
  EXPECT_TRUE(GetFlag(&helper->cpu_, kSF));  // Sign flag reflects result sign
  EXPECT_TRUE(
      GetFlag(&helper->cpu_, kOF));  // Overflow: positive - negative = overflow
}

TEST_F(StringRepCmpTest, REPECmpsbBackward) {
  // Test REPE CMPSB in backward direction
  auto helper = CPUTestHelper::CreateWithProgram(
      "repe-cmpsb-backward-test", "repe cmpsb\n");
  helper->cpu_.registers[kDS] = 0x020;
  helper->cpu_.registers[kES] = 0x030;
  helper->cpu_.registers[kSI] = 0x02;  // Start at offset 2
  helper->cpu_.registers[kDI] = 0x02;  // Start at offset 2
  helper->cpu_.registers[kCX] = 3;     // Compare 3 bytes

  // Set direction flag (backward direction)
  SetFlag(&helper->cpu_, kDF, true);

  // Set up source and destination data
  helper->memory_[0x202] = 0x55;  // Third byte matches
  helper->memory_[0x201] = 0x55;  // Second byte matches
  helper->memory_[0x200] = 0x66;  // First byte doesn't match - should stop here

  helper->memory_[0x302] = 0x55;  // Third byte matches
  helper->memory_[0x301] = 0x55;  // Second byte matches
  helper->memory_[0x300] = 0x77;  // First byte doesn't match - should stop here

  // Execute REPE CMPSB
  helper->ExecuteInstructions(1);

  // Should stop after checking byte at offset 0
  EXPECT_EQ(
      helper->cpu_.registers[kSI], 0xFFFF);  // Points to 0x200-1 (wrapped)
  EXPECT_EQ(
      helper->cpu_.registers[kDI], 0xFFFF);   // Points to 0x300-1 (wrapped)
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);  // All iterations completed
  EXPECT_FALSE(
      GetFlag(&helper->cpu_, kZF));  // ZF clear because last comparison failed
}

TEST_F(StringRepCmpTest, CMPSSegmentOverride) {
  // Test CMPSB with segment override
  auto helper = CPUTestHelper::CreateWithProgram(
      "cmpsb-segment-override-test", "es cmpsb\n");
  helper->cpu_.registers[kDS] = 0x020;
  helper->cpu_.registers[kES] = 0x030;
  helper->cpu_.registers[kSI] = 0x00;
  helper->cpu_.registers[kDI] = 0x00;

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source and destination data
  // With ES override, source should come from ES:SI instead of DS:SI
  helper->memory_[0x200] = 0x11;  // DS:SI (should not be used)
  helper->memory_[0x300] =
      0x42;  // ES:SI (source with override) and ES:DI (destination)

  // Execute ES: CMPSB
  helper->ExecuteInstructions(1);

  // Check that data was compared from ES:SI to ES:DI
  EXPECT_TRUE(GetFlag(&helper->cpu_, kZF));  // Should be equal (0x42 == 0x42)

  // Check that SI and DI were incremented by 1
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x01);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x01);
}