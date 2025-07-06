#include <gtest/gtest.h>

#include "./test_helpers.h"
#include "cpu.h"

using namespace std;

class StringRepTest : public ::testing::Test {};

TEST_F(StringRepTest, MOVSBBasic) {
  auto helper =
      CPUTestHelper::CreateWithProgram("execute-movsb-basic-test", "movsb\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kES] = 0;

  // Set up source and destination addresses
  helper->cpu_.registers[kSI] = 0x0400;  // Source at DS:SI
  helper->cpu_.registers[kDI] = 0x0500;  // Destination at ES:DI

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source data
  helper->memory_[0x0400] = 0x42;
  helper->memory_[0x0500] = 0x00;  // Clear destination

  // Execute MOVSB
  helper->ExecuteInstructions(1);

  // Check that data was copied
  EXPECT_EQ(helper->memory_[0x0500], 0x42);

  // Check that SI and DI were incremented by 1 (byte operation)
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x0401);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x0501);
}

TEST_F(StringRepTest, MOVSWBasic) {
  auto helper =
      CPUTestHelper::CreateWithProgram("execute-movsw-basic-test", "movsw\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kES] = 0;

  // Set up source and destination addresses
  helper->cpu_.registers[kSI] = 0x0400;  // Source at DS:SI
  helper->cpu_.registers[kDI] = 0x0500;  // Destination at ES:DI

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source data (little-endian word)
  helper->memory_[0x0400] = 0x34;  // LSB
  helper->memory_[0x0401] = 0x12;  // MSB
  helper->memory_[0x0500] = 0x00;  // Clear destination
  helper->memory_[0x0501] = 0x00;

  // Execute MOVSW
  helper->ExecuteInstructions(1);

  // Check that data was copied
  EXPECT_EQ(helper->memory_[0x0500], 0x34);  // LSB
  EXPECT_EQ(helper->memory_[0x0501], 0x12);  // MSB

  // Check that SI and DI were incremented by 2 (word operation)
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x0402);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x0502);
}

TEST_F(StringRepTest, MOVSBBackward) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-movsb-backward-test",
      "std\n"  // Set direction flag for backward direction
      "movsb\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kES] = 0;

  // Set up source and destination addresses
  helper->cpu_.registers[kSI] = 0x0400;  // Source at DS:SI
  helper->cpu_.registers[kDI] = 0x0500;  // Destination at ES:DI

  // Set up source data
  helper->memory_[0x0400] = 0x42;
  helper->memory_[0x0500] = 0x00;  // Clear destination

  // Execute STD then MOVSB
  helper->ExecuteInstructions(2);

  // Check that data was copied
  EXPECT_EQ(helper->memory_[0x0500], 0x42);

  // Check that SI and DI were decremented by 1 (byte operation, backward)
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x03FF);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x04FF);

  // Check that direction flag is set
  EXPECT_TRUE(GetFlag(&helper->cpu_, kDF));
}

TEST_F(StringRepTest, MOVSWBackward) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-movsw-backward-test",
      "std\n"  // Set direction flag for backward direction
      "movsw\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kES] = 0;

  // Set up source and destination addresses
  helper->cpu_.registers[kSI] = 0x0400;  // Source at DS:SI
  helper->cpu_.registers[kDI] = 0x0500;  // Destination at ES:DI

  // Set up source data (little-endian word)
  helper->memory_[0x0400] = 0x34;  // LSB
  helper->memory_[0x0401] = 0x12;  // MSB
  helper->memory_[0x0500] = 0x00;  // Clear destination
  helper->memory_[0x0501] = 0x00;

  // Execute STD then MOVSW
  helper->ExecuteInstructions(2);

  // Check that data was copied
  EXPECT_EQ(helper->memory_[0x0500], 0x34);  // LSB
  EXPECT_EQ(helper->memory_[0x0501], 0x12);  // MSB

  // Check that SI and DI were decremented by 2 (word operation, backward)
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x03FE);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x04FE);

  // Check that direction flag is set
  EXPECT_TRUE(GetFlag(&helper->cpu_, kDF));
}

TEST_F(StringRepTest, REPMOVSBBasic) {
  auto helper =
      CPUTestHelper::CreateWithProgram("execute-rep-movsb-test", "rep movsb\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kES] = 0;

  // Set up source and destination addresses
  helper->cpu_.registers[kSI] = 0x0400;  // Source at DS:SI
  helper->cpu_.registers[kDI] = 0x0500;  // Destination at ES:DI
  helper->cpu_.registers[kCX] = 5;       // Repeat count

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source data
  helper->memory_[0x0400] = 0x10;
  helper->memory_[0x0401] = 0x20;
  helper->memory_[0x0402] = 0x30;
  helper->memory_[0x0403] = 0x40;
  helper->memory_[0x0404] = 0x50;

  // Clear destination
  helper->memory_[0x0500] = 0x00;
  helper->memory_[0x0501] = 0x00;
  helper->memory_[0x0502] = 0x00;
  helper->memory_[0x0503] = 0x00;
  helper->memory_[0x0504] = 0x00;

  // Execute REP MOVSB
  helper->ExecuteInstructions(1);

  // Check that all bytes were copied
  EXPECT_EQ(helper->memory_[0x0500], 0x10);
  EXPECT_EQ(helper->memory_[0x0501], 0x20);
  EXPECT_EQ(helper->memory_[0x0502], 0x30);
  EXPECT_EQ(helper->memory_[0x0503], 0x40);
  EXPECT_EQ(helper->memory_[0x0504], 0x50);

  // Check that SI and DI were incremented by count (5 bytes)
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x0405);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x0505);

  // Check that CX was decremented to 0
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);
}

TEST_F(StringRepTest, REPMOVSWBasic) {
  auto helper =
      CPUTestHelper::CreateWithProgram("execute-rep-movsw-test", "rep movsw\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kES] = 0;

  // Set up source and destination addresses
  helper->cpu_.registers[kSI] = 0x0400;  // Source at DS:SI
  helper->cpu_.registers[kDI] = 0x0500;  // Destination at ES:DI
  helper->cpu_.registers[kCX] = 3;       // Repeat count (3 words)

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source data (little-endian words)
  helper->memory_[0x0400] = 0x34;  // Word 1 LSB
  helper->memory_[0x0401] = 0x12;  // Word 1 MSB
  helper->memory_[0x0402] = 0x78;  // Word 2 LSB
  helper->memory_[0x0403] = 0x56;  // Word 2 MSB
  helper->memory_[0x0404] = 0xBC;  // Word 3 LSB
  helper->memory_[0x0405] = 0x9A;  // Word 3 MSB

  // Clear destination
  for (int i = 0; i < 6; i++) {
    helper->memory_[0x0500 + i] = 0x00;
  }

  // Execute REP MOVSW
  helper->ExecuteInstructions(1);

  // Check that all words were copied
  EXPECT_EQ(helper->memory_[0x0500], 0x34);  // Word 1 LSB
  EXPECT_EQ(helper->memory_[0x0501], 0x12);  // Word 1 MSB
  EXPECT_EQ(helper->memory_[0x0502], 0x78);  // Word 2 LSB
  EXPECT_EQ(helper->memory_[0x0503], 0x56);  // Word 2 MSB
  EXPECT_EQ(helper->memory_[0x0504], 0xBC);  // Word 3 LSB
  EXPECT_EQ(helper->memory_[0x0505], 0x9A);  // Word 3 MSB

  // Check that SI and DI were incremented by count * 2 (3 words = 6 bytes)
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x0406);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x0506);

  // Check that CX was decremented to 0
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);
}

TEST_F(StringRepTest, REPMOVSBBackward) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-rep-movsb-backward-test",
      "std\n"  // Set direction flag for backward direction
      "rep movsb\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kES] = 0;

  // Set up source and destination addresses (point to last byte)
  helper->cpu_.registers[kSI] = 0x0404;  // Source at DS:SI (last byte)
  helper->cpu_.registers[kDI] = 0x0504;  // Destination at ES:DI (last byte)
  helper->cpu_.registers[kCX] = 5;       // Repeat count

  // Set up source data
  helper->memory_[0x0400] = 0x10;
  helper->memory_[0x0401] = 0x20;
  helper->memory_[0x0402] = 0x30;
  helper->memory_[0x0403] = 0x40;
  helper->memory_[0x0404] = 0x50;

  // Clear destination
  helper->memory_[0x0500] = 0x00;
  helper->memory_[0x0501] = 0x00;
  helper->memory_[0x0502] = 0x00;
  helper->memory_[0x0503] = 0x00;
  helper->memory_[0x0504] = 0x00;

  // Execute STD then REP MOVSB
  helper->ExecuteInstructions(2);

  // Check that all bytes were copied (in reverse order)
  EXPECT_EQ(helper->memory_[0x0500], 0x10);
  EXPECT_EQ(helper->memory_[0x0501], 0x20);
  EXPECT_EQ(helper->memory_[0x0502], 0x30);
  EXPECT_EQ(helper->memory_[0x0503], 0x40);
  EXPECT_EQ(helper->memory_[0x0504], 0x50);

  // Check that SI and DI were decremented by count (5 bytes)
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x03FF);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x04FF);

  // Check that CX was decremented to 0
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);
}

TEST_F(StringRepTest, REPMOVSBZeroCount) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-rep-movsb-zero-test", "rep movsb\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kES] = 0;

  // Set up source and destination addresses
  helper->cpu_.registers[kSI] = 0x0400;  // Source at DS:SI
  helper->cpu_.registers[kDI] = 0x0500;  // Destination at ES:DI
  helper->cpu_.registers[kCX] = 0;       // Zero repeat count

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source data
  helper->memory_[0x0400] = 0x42;
  helper->memory_[0x0500] = 0x00;  // Destination should remain unchanged

  // Execute REP MOVSB
  helper->ExecuteInstructions(1);

  // Check that no data was copied (destination unchanged)
  EXPECT_EQ(helper->memory_[0x0500], 0x00);

  // Check that SI and DI were not modified
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x0400);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x0500);

  // Check that CX remains 0
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);
}

TEST_F(StringRepTest, MOVSBSegmentOverride) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-movsb-segment-test",
      "es movsb\n");  // ES segment override for source
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kES] = 0;

  // Set up source and destination addresses
  helper->cpu_.registers[kSI] = 0x0400;  // Source at ES:SI (with override)
  helper->cpu_.registers[kDI] = 0x0500;  // Destination at ES:DI

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source data at ES:SI
  helper->memory_[0x0400] = 0x42;  // Source data
  helper->memory_[0x0500] = 0x00;  // Clear destination

  // Execute MOVSB with ES segment override
  helper->ExecuteInstructions(1);

  // Check that data was copied from ES:SI to ES:DI
  EXPECT_EQ(helper->memory_[0x0500], 0x42);

  // Check that SI and DI were incremented by 1
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x0401);
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x0501);
}

TEST_F(StringRepTest, MOVSBNoFlagsAffected) {
  auto helper =
      CPUTestHelper::CreateWithProgram("execute-movsb-flags-test", "movsb\n");
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kES] = 0;

  // Set up source and destination addresses
  helper->cpu_.registers[kSI] = 0x0400;
  helper->cpu_.registers[kDI] = 0x0500;

  // Set various flags before the operation
  SetFlag(&helper->cpu_, kCF, true);
  SetFlag(&helper->cpu_, kZF, true);
  SetFlag(&helper->cpu_, kSF, true);
  SetFlag(&helper->cpu_, kPF, true);
  SetFlag(&helper->cpu_, kOF, true);
  SetFlag(&helper->cpu_, kAF, true);
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source data
  helper->memory_[0x0400] = 0x42;

  // Execute MOVSB
  helper->ExecuteInstructions(1);

  // Check that MOVS instruction doesn't affect arithmetic flags
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true},
       {kDF, false}});  // DF should remain unchanged
}

// ============================================================================
// STOS (Store String) instruction tests
// ============================================================================

TEST_F(StringRepTest, STOSBBasic) {
  auto helper =
      CPUTestHelper::CreateWithProgram("execute-stosb-basic-test", "stosb\n");
  helper->cpu_.registers[kES] = 0;

  // Set up destination address and source value
  helper->cpu_.registers[kDI] = 0x0400;  // Destination at ES:DI
  helper->cpu_.registers[kAX] = 0x1242;  // AL = 0x42

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Clear destination
  helper->memory_[0x0400] = 0x00;

  // Execute STOSB
  helper->ExecuteInstructions(1);

  // Check that AL was stored at ES:DI
  EXPECT_EQ(helper->memory_[0x0400], 0x42);

  // Check that DI was incremented by 1 (byte operation)
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x0401);

  // Check that AL is unchanged
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x42);
}

TEST_F(StringRepTest, STOSWBasic) {
  auto helper =
      CPUTestHelper::CreateWithProgram("execute-stosw-basic-test", "stosw\n");
  helper->cpu_.registers[kES] = 0;

  // Set up destination address and source value
  helper->cpu_.registers[kDI] = 0x0400;  // Destination at ES:DI
  helper->cpu_.registers[kAX] = 0x1234;  // AX = 0x1234

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Clear destination
  helper->memory_[0x0400] = 0x00;
  helper->memory_[0x0401] = 0x00;

  // Execute STOSW
  helper->ExecuteInstructions(1);

  // Check that AX was stored at ES:DI (little-endian)
  EXPECT_EQ(helper->memory_[0x0400], 0x34);  // LSB
  EXPECT_EQ(helper->memory_[0x0401], 0x12);  // MSB

  // Check that DI was incremented by 2 (word operation)
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x0402);

  // Check that AX is unchanged
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1234);
}

TEST_F(StringRepTest, STOSBBackward) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-stosb-backward-test",
      "std\n"  // Set direction flag for backward direction
      "stosb\n");
  helper->cpu_.registers[kES] = 0;

  // Set up destination address and source value
  helper->cpu_.registers[kDI] = 0x0400;  // Destination at ES:DI
  helper->cpu_.registers[kAX] = 0x5678;  // AL = 0x78

  // Clear destination
  helper->memory_[0x0400] = 0x00;

  // Execute STD then STOSB
  helper->ExecuteInstructions(2);

  // Check that AL was stored at ES:DI
  EXPECT_EQ(helper->memory_[0x0400], 0x78);

  // Check that DI was decremented by 1 (byte operation, backward)
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x03FF);

  // Check that direction flag is set
  EXPECT_TRUE(GetFlag(&helper->cpu_, kDF));
}

TEST_F(StringRepTest, STOSWBackward) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-stosw-backward-test",
      "std\n"  // Set direction flag for backward direction
      "stosw\n");
  helper->cpu_.registers[kES] = 0;

  // Set up destination address and source value
  helper->cpu_.registers[kDI] = 0x0400;  // Destination at ES:DI
  helper->cpu_.registers[kAX] = 0x9ABC;  // AX = 0x9ABC

  // Clear destination
  helper->memory_[0x0400] = 0x00;
  helper->memory_[0x0401] = 0x00;

  // Execute STD then STOSW
  helper->ExecuteInstructions(2);

  // Check that AX was stored at ES:DI (little-endian)
  EXPECT_EQ(helper->memory_[0x0400], 0xBC);  // LSB
  EXPECT_EQ(helper->memory_[0x0401], 0x9A);  // MSB

  // Check that DI was decremented by 2 (word operation, backward)
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x03FE);

  // Check that direction flag is set
  EXPECT_TRUE(GetFlag(&helper->cpu_, kDF));
}

TEST_F(StringRepTest, REPSTOSBBasic) {
  auto helper =
      CPUTestHelper::CreateWithProgram("execute-rep-stosb-test", "rep stosb\n");
  helper->cpu_.registers[kES] = 0;

  // Set up destination address, source value, and repeat count
  helper->cpu_.registers[kDI] = 0x0300;  // Destination at ES:DI
  helper->cpu_.registers[kAX] = 0x00AA;  // AL = 0xAA
  helper->cpu_.registers[kCX] = 5;       // Repeat count

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Clear destination
  for (int i = 0; i < 5; i++) {
    helper->memory_[0x0300 + i] = 0x00;
  }

  // Execute REP STOSB
  helper->ExecuteInstructions(1);

  // Check that all bytes were filled with AL
  EXPECT_EQ(helper->memory_[0x0300], 0xAA);
  EXPECT_EQ(helper->memory_[0x0301], 0xAA);
  EXPECT_EQ(helper->memory_[0x0302], 0xAA);
  EXPECT_EQ(helper->memory_[0x0303], 0xAA);
  EXPECT_EQ(helper->memory_[0x0304], 0xAA);

  // Check that DI was incremented by count (5 bytes)
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x0305);

  // Check that CX was decremented to 0
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);
}

TEST_F(StringRepTest, REPSTOSWBasic) {
  auto helper =
      CPUTestHelper::CreateWithProgram("execute-rep-stosw-test", "rep stosw\n");
  helper->cpu_.registers[kES] = 0;

  // Set up destination address, source value, and repeat count
  helper->cpu_.registers[kDI] = 0x0300;  // Destination at ES:DI
  helper->cpu_.registers[kAX] = 0xDEAD;  // AX = 0xDEAD
  helper->cpu_.registers[kCX] = 3;       // Repeat count (3 words)

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Clear destination
  for (int i = 0; i < 6; i++) {
    helper->memory_[0x0300 + i] = 0x00;
  }

  // Execute REP STOSW
  helper->ExecuteInstructions(1);

  // Check that all words were filled with AX (little-endian)
  EXPECT_EQ(helper->memory_[0x0300], 0xAD);  // Word 1 LSB
  EXPECT_EQ(helper->memory_[0x0301], 0xDE);  // Word 1 MSB
  EXPECT_EQ(helper->memory_[0x0302], 0xAD);  // Word 2 LSB
  EXPECT_EQ(helper->memory_[0x0303], 0xDE);  // Word 2 MSB
  EXPECT_EQ(helper->memory_[0x0304], 0xAD);  // Word 3 LSB
  EXPECT_EQ(helper->memory_[0x0305], 0xDE);  // Word 3 MSB

  // Check that DI was incremented by count * 2 (3 words = 6 bytes)
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x0306);

  // Check that CX was decremented to 0
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);
}

TEST_F(StringRepTest, REPSTOSBBackward) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-rep-stosb-backward-test",
      "std\n"  // Set direction flag for backward direction
      "rep stosb\n");
  helper->cpu_.registers[kES] = 0;

  // Set up destination address (point to last byte), source value, and repeat
  // count
  helper->cpu_.registers[kDI] = 0x0304;  // Destination at ES:DI (last byte)
  helper->cpu_.registers[kAX] = 0x00BB;  // AL = 0xBB
  helper->cpu_.registers[kCX] = 5;       // Repeat count

  // Clear destination
  for (int i = 0; i < 5; i++) {
    helper->memory_[0x0300 + i] = 0x00;
  }

  // Execute STD then REP STOSB
  helper->ExecuteInstructions(2);

  // Check that all bytes were filled with AL (in reverse order)
  EXPECT_EQ(helper->memory_[0x0300], 0xBB);
  EXPECT_EQ(helper->memory_[0x0301], 0xBB);
  EXPECT_EQ(helper->memory_[0x0302], 0xBB);
  EXPECT_EQ(helper->memory_[0x0303], 0xBB);
  EXPECT_EQ(helper->memory_[0x0304], 0xBB);

  // Check that DI was decremented by count (5 bytes)
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x02FF);

  // Check that CX was decremented to 0
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);
}

TEST_F(StringRepTest, REPSTOSBZeroCount) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-rep-stosb-zero-test", "rep stosb\n");
  helper->cpu_.registers[kES] = 0;

  // Set up destination address and source value
  helper->cpu_.registers[kDI] = 0x0300;  // Destination at ES:DI
  helper->cpu_.registers[kAX] = 0x00CC;  // AL = 0xCC
  helper->cpu_.registers[kCX] = 0;       // Zero repeat count

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up destination data
  helper->memory_[0x0300] = 0x99;  // Should remain unchanged

  // Execute REP STOSB
  helper->ExecuteInstructions(1);

  // Check that no data was stored (destination unchanged)
  EXPECT_EQ(helper->memory_[0x0300], 0x99);

  // Check that DI was not modified
  EXPECT_EQ(helper->cpu_.registers[kDI], 0x0300);

  // Check that CX remains 0
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);
}

TEST_F(StringRepTest, STOSBNoFlagsAffected) {
  auto helper =
      CPUTestHelper::CreateWithProgram("execute-stosb-flags-test", "stosb\n");
  helper->cpu_.registers[kES] = 0;

  // Set up destination address and source value
  helper->cpu_.registers[kDI] = 0x0300;
  helper->cpu_.registers[kAX] = 0x00DD;  // AL = 0xDD

  // Set various flags before the operation
  SetFlag(&helper->cpu_, kCF, true);
  SetFlag(&helper->cpu_, kZF, true);
  SetFlag(&helper->cpu_, kSF, true);
  SetFlag(&helper->cpu_, kPF, true);
  SetFlag(&helper->cpu_, kOF, true);
  SetFlag(&helper->cpu_, kAF, true);
  SetFlag(&helper->cpu_, kDF, false);

  // Execute STOSB
  helper->ExecuteInstructions(1);

  // Check that STOS instruction doesn't affect arithmetic flags
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true},
       {kDF, false}});  // DF should remain unchanged
}

// ============================================================================
// LODS (Load String) instruction tests
// ============================================================================

TEST_F(StringRepTest, LODSBBasic) {
  auto helper =
      CPUTestHelper::CreateWithProgram("execute-lodsb-basic-test", "lodsb\n");
  helper->cpu_.registers[kDS] = 0;

  // Set up source address
  helper->cpu_.registers[kSI] = 0x0400;  // Source at DS:SI

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source data
  helper->memory_[0x0400] = 0x42;

  // Clear AL register
  helper->cpu_.registers[kAX] = 0x0000;

  // Execute LODSB
  helper->ExecuteInstructions(1);

  // Check that data was loaded into AL
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x42);
  EXPECT_EQ((helper->cpu_.registers[kAX] >> 8) & 0xFF, 0x00);  // AH unchanged

  // Check that SI was incremented by 1 (byte operation)
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x0401);
}

TEST_F(StringRepTest, LODSWBasic) {
  auto helper =
      CPUTestHelper::CreateWithProgram("execute-lodsw-basic-test", "lodsw\n");
  helper->cpu_.registers[kDS] = 0;

  // Set up source address
  helper->cpu_.registers[kSI] = 0x0400;  // Source at DS:SI

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source data (little-endian word)
  helper->memory_[0x0400] = 0x34;  // LSB
  helper->memory_[0x0401] = 0x12;  // MSB

  // Clear AX register
  helper->cpu_.registers[kAX] = 0x0000;

  // Execute LODSW
  helper->ExecuteInstructions(1);

  // Check that data was loaded into AX
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1234);

  // Check that SI was incremented by 2 (word operation)
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x0402);
}

TEST_F(StringRepTest, LODSBBackward) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-lodsb-backward-test",
      "std\n"  // Set direction flag for backward direction
      "lodsb\n");
  helper->cpu_.registers[kDS] = 0;

  // Set up source address
  helper->cpu_.registers[kSI] = 0x0400;  // Source at DS:SI

  // Set up source data
  helper->memory_[0x0400] = 0x42;

  // Clear AX register
  helper->cpu_.registers[kAX] = 0x0000;

  // Execute STD then LODSB
  helper->ExecuteInstructions(2);

  // Check that data was loaded into AL
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x42);

  // Check that SI was decremented by 1 (byte operation, backward)
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x03FF);

  // Check that direction flag is set
  EXPECT_TRUE(GetFlag(&helper->cpu_, kDF));
}

TEST_F(StringRepTest, LODSWBackward) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-lodsw-backward-test",
      "std\n"  // Set direction flag for backward direction
      "lodsw\n");
  helper->cpu_.registers[kDS] = 0;

  // Set up source address
  helper->cpu_.registers[kSI] = 0x0400;  // Source at DS:SI

  // Set up source data (little-endian word)
  helper->memory_[0x0400] = 0x34;  // LSB
  helper->memory_[0x0401] = 0x12;  // MSB

  // Clear AX register
  helper->cpu_.registers[kAX] = 0x0000;

  // Execute STD then LODSW
  helper->ExecuteInstructions(2);

  // Check that data was loaded into AX
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x1234);

  // Check that SI was decremented by 2 (word operation, backward)
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x03FE);

  // Check that direction flag is set
  EXPECT_TRUE(GetFlag(&helper->cpu_, kDF));
}

TEST_F(StringRepTest, LODSBSegmentOverride) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-lodsb-segment-test",
      "es lodsb\n");  // ES segment override for source
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kES] = 0;

  // Set up source address
  helper->cpu_.registers[kSI] = 0x0400;  // Source at ES:SI (with override)

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source data at ES:SI
  helper->memory_[0x0400] = 0x42;  // Source data

  // Clear AX register
  helper->cpu_.registers[kAX] = 0x0000;

  // Execute LODSB with ES segment override
  helper->ExecuteInstructions(1);

  // Check that data was loaded from ES:SI into AL
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x42);

  // Check that SI was incremented by 1
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x0401);
}

TEST_F(StringRepTest, LODSBNoFlagsAffected) {
  auto helper =
      CPUTestHelper::CreateWithProgram("execute-lodsb-flags-test", "lodsb\n");
  helper->cpu_.registers[kDS] = 0;

  // Set up source address
  helper->cpu_.registers[kSI] = 0x0400;

  // Set various flags before the operation
  SetFlag(&helper->cpu_, kCF, true);
  SetFlag(&helper->cpu_, kZF, true);
  SetFlag(&helper->cpu_, kSF, true);
  SetFlag(&helper->cpu_, kPF, true);
  SetFlag(&helper->cpu_, kOF, true);
  SetFlag(&helper->cpu_, kAF, true);
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source data
  helper->memory_[0x0400] = 0x42;

  // Execute LODSB
  helper->ExecuteInstructions(1);

  // Check that LODS instruction doesn't affect arithmetic flags
  helper->CheckFlags(
      {{kCF, true},
       {kZF, true},
       {kSF, true},
       {kPF, true},
       {kOF, true},
       {kAF, true},
       {kDF, false}});  // DF should remain unchanged
}

TEST_F(StringRepTest, REPLODSBBasic) {
  auto helper =
      CPUTestHelper::CreateWithProgram("execute-rep-lodsb-test", "rep lodsb\n");
  helper->cpu_.registers[kDS] = 0;

  // Set up source address and repeat count
  helper->cpu_.registers[kSI] = 0x0400;  // Source at DS:SI
  helper->cpu_.registers[kCX] = 5;       // Repeat count

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source data
  helper->memory_[0x0400] = 0x10;
  helper->memory_[0x0401] = 0x20;
  helper->memory_[0x0402] = 0x30;
  helper->memory_[0x0403] = 0x40;
  helper->memory_[0x0404] = 0x50;

  // Clear AX register
  helper->cpu_.registers[kAX] = 0x0000;

  // Execute REP LODSB
  helper->ExecuteInstructions(1);

  // Check that the last byte was loaded into AL (0x50)
  // Note: REP LODSB loads each byte but only the last one remains in AL
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x50);

  // Check that SI was incremented by count (5 bytes)
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x0405);

  // Check that CX was decremented to 0
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);
}

TEST_F(StringRepTest, REPLODSWBasic) {
  auto helper =
      CPUTestHelper::CreateWithProgram("execute-rep-lodsw-test", "rep lodsw\n");
  helper->cpu_.registers[kDS] = 0;

  // Set up source address and repeat count
  helper->cpu_.registers[kSI] = 0x0400;  // Source at DS:SI
  helper->cpu_.registers[kCX] = 3;       // Repeat count (3 words)

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source data (little-endian words)
  helper->memory_[0x0400] = 0x34;  // Word 1 LSB
  helper->memory_[0x0401] = 0x12;  // Word 1 MSB
  helper->memory_[0x0402] = 0x78;  // Word 2 LSB
  helper->memory_[0x0403] = 0x56;  // Word 2 MSB
  helper->memory_[0x0404] = 0xBC;  // Word 3 LSB
  helper->memory_[0x0405] = 0x9A;  // Word 3 MSB

  // Clear AX register
  helper->cpu_.registers[kAX] = 0x0000;

  // Execute REP LODSW
  helper->ExecuteInstructions(1);

  // Check that the last word was loaded into AX (0x9ABC)
  // Note: REP LODSW loads each word but only the last one remains in AX
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x9ABC);

  // Check that SI was incremented by count * 2 (3 words = 6 bytes)
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x0406);

  // Check that CX was decremented to 0
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);
}

TEST_F(StringRepTest, REPLODSBBackward) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-rep-lodsb-backward-test",
      "std\n"  // Set direction flag for backward direction
      "rep lodsb\n");
  helper->cpu_.registers[kDS] = 0;

  // Set up source address (point to last byte) and repeat count
  helper->cpu_.registers[kSI] = 0x0404;  // Source at DS:SI (last byte)
  helper->cpu_.registers[kCX] = 5;       // Repeat count

  // Set up source data
  helper->memory_[0x0400] = 0x10;
  helper->memory_[0x0401] = 0x20;
  helper->memory_[0x0402] = 0x30;
  helper->memory_[0x0403] = 0x40;
  helper->memory_[0x0404] = 0x50;

  // Clear AX register
  helper->cpu_.registers[kAX] = 0x0000;

  // Execute STD then REP LODSB
  helper->ExecuteInstructions(2);

  // Check that the last byte processed was loaded into AL (0x10)
  // Note: Processing backwards from 0x404 to 0x400, so last byte is 0x10
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0x10);

  // Check that SI was decremented by count (5 bytes)
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x03FF);

  // Check that CX was decremented to 0
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);
}

TEST_F(StringRepTest, REPLODSBZeroCount) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-rep-lodsb-zero-test", "rep lodsb\n");
  helper->cpu_.registers[kDS] = 0;

  // Set up source address
  helper->cpu_.registers[kSI] = 0x0400;  // Source at DS:SI
  helper->cpu_.registers[kCX] = 0;       // Zero repeat count

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source data
  helper->memory_[0x0400] = 0x42;

  // Set AX register to a known value
  helper->cpu_.registers[kAX] = 0x9999;

  // Execute REP LODSB
  helper->ExecuteInstructions(1);

  // Check that AL was not changed (no iterations)
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x9999);

  // Check that SI was not modified
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x0400);

  // Check that CX remains 0
  EXPECT_EQ(helper->cpu_.registers[kCX], 0);
}

TEST_F(StringRepTest, LODSBMultipleOperations) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-lodsb-multiple-test",
      "lodsb\n"
      "lodsb\n"
      "lodsb\n");
  helper->cpu_.registers[kDS] = 0;

  // Set up source address
  helper->cpu_.registers[kSI] = 0x0300;  // Source at DS:SI

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source data
  helper->memory_[0x0300] = 0xAA;
  helper->memory_[0x0301] = 0xBB;
  helper->memory_[0x0302] = 0xCC;

  // Clear AX register
  helper->cpu_.registers[kAX] = 0x0000;

  // Execute first LODSB
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0xAA);
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x0301);

  // Execute second LODSB
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0xBB);
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x0302);

  // Execute third LODSB
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX] & 0xFF, 0xCC);
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x0303);
}

TEST_F(StringRepTest, LODSWMultipleOperations) {
  auto helper = CPUTestHelper::CreateWithProgram(
      "execute-lodsw-multiple-test",
      "lodsw\n"
      "lodsw\n");
  helper->cpu_.registers[kDS] = 0;

  // Set up source address
  helper->cpu_.registers[kSI] = 0x0300;  // Source at DS:SI

  // Clear direction flag (forward direction)
  SetFlag(&helper->cpu_, kDF, false);

  // Set up source data (little-endian words)
  helper->memory_[0x0300] = 0x11;  // Word 1 LSB
  helper->memory_[0x0301] = 0x22;  // Word 1 MSB
  helper->memory_[0x0302] = 0x33;  // Word 2 LSB
  helper->memory_[0x0303] = 0x44;  // Word 2 MSB

  // Clear AX register
  helper->cpu_.registers[kAX] = 0x0000;

  // Execute first LODSW
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x2211);
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x0302);

  // Execute second LODSW
  helper->ExecuteInstructions(1);
  EXPECT_EQ(helper->cpu_.registers[kAX], 0x4433);
  EXPECT_EQ(helper->cpu_.registers[kSI], 0x0304);
}
