#include <gtest/gtest.h>

#include <algorithm>

#define YAX86_IMPLEMENTATION
#include "cpu.h"

using namespace std;

class OpcodeTableTest : public ::testing::Test {};

TEST_F(OpcodeTableTest, MetadataIntegrity) {
  EXPECT_EQ(sizeof(YCPUOpcodeMetadata), sizeof(void*) == 4 ? 8 : 16);
  EXPECT_EQ(sizeof(YCPUOpcodeTable), 256 * sizeof(YCPUOpcodeMetadata));

  for (int i = 0; i < 256; ++i) {
    const YCPUOpcodeMetadata& metadata = YCPUOpcodeTable[i];

    if (metadata.handler == YCPUExecuteInvalidOpcode) {
      continue;
    }

    // Width should be either Y_CPU_BYTE or Y_CPU_WORD
    EXPECT_TRUE(metadata.width == Y_CPU_BYTE || metadata.width == Y_CPU_WORD)
        << "Invalid width for opcode 0x" << hex << i;

    // Immediate size for all instructions should be between 0 and 2, except
    // long jump and long call which have an immediate size of 4.
    if (i == 0xEA || i == 0x9A) {
      EXPECT_TRUE(metadata.immediate_size == 4)
          << "Invalid immediate size for opcode 0x" << hex << i;
    } else {
      EXPECT_TRUE(metadata.immediate_size <= 2)
          << "Invalid immediate size for opcode 0x" << hex << i;
    }
  }
}

TEST_F(OpcodeTableTest, InstructionPrefixMetadataIntegrity) {
  const vector<YCPUInstructionPrefix> prefixes = {
      Y_CPU_PREFIX_ES,   Y_CPU_PREFIX_CS,    Y_CPU_PREFIX_SS,  Y_CPU_PREFIX_DS,
      Y_CPU_PREFIX_LOCK, Y_CPU_PREFIX_REPNZ, Y_CPU_PREFIX_REP,
  };
  for (const auto& prefix : prefixes) {
    const YCPUOpcodeMetadata& metadata = YCPUOpcodeTable[prefix];
    EXPECT_EQ(metadata.handler, YCPUExecuteInvalidOpcode)
        << "Prefix opcode 0x" << hex << prefix
        << " should be handled by YCPUExecuteInvalidOpcode";
  }
}

TEST_F(OpcodeTableTest, EveryOpcodeByteDecodes) {
  // The 8086/8088 has no invalid opcode exception - every one of the 256
  // opcode bytes decodes to something. The only entries that do not name an
  // instruction are the prefixes, which the prefix decoder consumes before an
  // opcode is ever looked up.
  static const uint8_t kPrefixes[] = {0x26, 0x2E, 0x36, 0x3E,
                                      0xF0, 0xF1, 0xF2, 0xF3};

  for (int opcode = 0; opcode < 256; ++opcode) {
    const bool is_prefix =
        find(begin(kPrefixes), end(kPrefixes), opcode) != end(kPrefixes);
    EXPECT_EQ(
        YCPUOpcodeTable[opcode].handler == YCPUExecuteInvalidOpcode, is_prefix)
        << "opcode 0x" << hex << opcode;
  }
}

TEST_F(OpcodeTableTest, EveryEntryHasAHandler) {
  // YCPUTick() calls through the handler without checking it first, which is
  // only safe because every entry in the table has one. A new entry that left
  // the field out would be a null call rather than an invalid instruction.
  for (int opcode = 0; opcode < 256; ++opcode) {
    EXPECT_NE(YCPUOpcodeTable[opcode].handler, nullptr)
        << "opcode 0x" << hex << opcode;
  }
}
