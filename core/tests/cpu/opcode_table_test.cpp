#include <gtest/gtest.h>

#include <algorithm>

#define YAX86_IMPLEMENTATION
#include "cpu.h"

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

TEST_F(OpcodeTableTest, EveryOpcodeByteDecodes) {
  // The 8086/8088 has no invalid opcode exception - every one of the 256
  // opcode bytes decodes to something. The only entries without a handler are
  // the prefixes, which the prefix decoder consumes before an opcode is ever
  // looked up.
  static const uint8_t kPrefixes[] = {0x26, 0x2E, 0x36, 0x3E,
                                      0xF0, 0xF1, 0xF2, 0xF3};

  for (int opcode = 0; opcode < 256; ++opcode) {
    const bool is_prefix =
        find(begin(kPrefixes), end(kPrefixes), opcode) != end(kPrefixes);
    EXPECT_EQ(opcode_table[opcode].handler != nullptr, !is_prefix)
        << "opcode 0x" << hex << opcode;
  }
}
