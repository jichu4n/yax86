#include <gtest/gtest.h>

#define YAX86_IMPLEMENTATION
#include "../cpu.h"

using namespace std;

class OpcodeTableTest : public ::testing::Test {};

TEST_F(OpcodeTableTest, MetadataIntegrity) {
  EXPECT_EQ(sizeof(opcode_table), 256 * sizeof(OpcodeMetadata));

  for (int i = 0; i < 256; ++i) {
    const OpcodeMetadata& metadata = opcode_table[i];
    // Check opcode == index
    EXPECT_EQ(metadata.opcode, i) << "Opcode mismatch at index 0x" << hex << i;

    if (metadata.handler == nullptr) {
      continue;
    }

    // Width should be either kByte or kWord
    EXPECT_TRUE(metadata.width == kByte || metadata.width == kWord)
        << "Invalid width for opcode 0x" << hex << i;

    // Immediate size for all instructions should be between 0 and 2, except
    // long jump and long call which have an immediate size of 4.
    if (metadata.opcode == 0xEA || metadata.opcode == 0x9A) {
      EXPECT_TRUE(metadata.immediate_size == 4)
          << "Invalid immediate size for opcode 0x" << hex << i;
    } else {
      EXPECT_TRUE(metadata.immediate_size >= 0 && metadata.immediate_size <= 2)
          << "Invalid immediate size for opcode 0x" << hex << i;
    }
  }
}

TEST_F(OpcodeTableTest, InstructionPrefixMetadataIntegrity) {
  const vector<InstructionPrefix> prefixes = {
      kPrefixES,   kPrefixCS,    kPrefixSS,  kPrefixDS,
      kPrefixLOCK, kPrefixREPNZ, kPrefixREP,
  };
  for (const auto& prefix : prefixes) {
    const OpcodeMetadata& metadata = opcode_table[prefix];
    EXPECT_EQ(metadata.opcode, prefix)
        << "Opcode mismatch at index 0x" << hex << prefix;
    EXPECT_EQ(metadata.handler, nullptr)
        << "Handler should be null for prefix opcode 0x" << hex << prefix;
  }
}
