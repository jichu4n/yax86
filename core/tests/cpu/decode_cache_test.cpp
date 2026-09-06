#include <vector>

#include "cpu.h"
#include "gtest/gtest.h"

namespace {

// Two 64KB segments, so that a segment wrap has somewhere to wrap to.
constexpr uint32_t kMemorySize = 0x20000;
// Small enough that two addresses can be made to collide on purpose, and a
// power of two as CPUSetDecodeCache() requires.
constexpr uint32_t kNumCacheEntries = 16;

constexpr uint32_t kProgramAddress = 0x0100;

enum : uint8_t {
  // MOV AL, imm8
  kOpMovAlImm8 = 0xB0,
  // MOV BL, imm8
  kOpMovBlImm8 = 0xB3,
  // INC BX
  kOpIncBx = 0x43,
  // MOV AL, moffs8
  kOpMovAlMoffs8 = 0xA0,
  // ES segment override prefix
  kPrefixES = 0x26,
};

// A cached decode is not observable from the outside, so every test here makes
// it observable the same way: change the instruction bytes without telling the
// CPU, and see which instruction runs. The old one means the decode was
// reused; the new one means it was not.
//
// Poking memory behind the CPU's back is exactly what a host must not do, and
// is why CPUNotifyMemoryWrite() exists. It is the lever these tests need
// precisely because it is the one thing the cache cannot see.
class DecodeCacheTest : public ::testing::Test {
 protected:
  void SetUp() override {
    memory_.assign(kMemorySize, 0);
    config_ = CPUConfig{};
    config_.context = this;
    config_.read_memory_byte = ReadMemoryByte;
    config_.write_memory_byte = WriteMemoryByte;
    config_.get_instruction_fetch_window = GetInstructionFetchWindow;
    CPUInit(&cpu_, &config_);
    CPUSetDirectDataWindow(&cpu_, memory_.data(), kMemorySize);
    CPUSetDecodeCache(&cpu_, cache_, kNumCacheEntries);
    cpu_.registers[kCS] = 0;
    cpu_.registers[kDS] = 0;
    cpu_.registers[kSS] = 0;
    cpu_.registers[kSP] = 0xFFFE;
    cpu_.registers[kIP] = kProgramAddress;
  }

  // Writes instruction bytes the way a host is supposed to - the CPU is told,
  // so anything decoded from them is discarded.
  void Write(uint32_t address, const std::vector<uint8_t>& bytes) {
    for (size_t i = 0; i < bytes.size(); ++i) {
      CPUNotifyMemoryWrite(&cpu_, address + i);
      memory_[address + i] = bytes[i];
    }
  }

  // Writes instruction bytes without telling the CPU, which is what makes a
  // reused decode visible.
  void PokeBehindTheCPUsBack(
      uint32_t address, const std::vector<uint8_t>& bytes) {
    for (size_t i = 0; i < bytes.size(); ++i) {
      memory_[address + i] = bytes[i];
    }
  }

  // Runs one instruction from address, leaving IP wherever it ends up.
  void RunAt(uint16_t ip) {
    cpu_.registers[kIP] = ip;
    ASSERT_EQ(CPUTick(&cpu_), kCPUTickExecuted);
  }

  static uint8_t ReadMemoryByte(CPUState* cpu, uint32_t address) {
    DecodeCacheTest* self = static_cast<DecodeCacheTest*>(cpu->config->context);
    return address < kMemorySize ? self->memory_[address] : 0xFF;
  }

  static void WriteMemoryByte(CPUState* cpu, uint32_t address, uint8_t value) {
    DecodeCacheTest* self = static_cast<DecodeCacheTest*>(cpu->config->context);
    if (address < kMemorySize) {
      self->memory_[address] = value;
    }
  }

  static void GetInstructionFetchWindow(CPUState* cpu, uint32_t address) {
    DecodeCacheTest* self = static_cast<DecodeCacheTest*>(cpu->config->context);
    if (address >= kMemorySize) {
      cpu->instruction_fetch_window.data = nullptr;
      return;
    }
    cpu->instruction_fetch_window.data = self->memory_.data();
    cpu->instruction_fetch_window.start = 0;
    cpu->instruction_fetch_window.end = kMemorySize;
  }

  std::vector<uint8_t> memory_;
  CPUConfig config_ = {};
  CPUState cpu_ = {};
  CPUDecodeCacheEntry cache_[kNumCacheEntries] = {};
};

TEST_F(DecodeCacheTest, ARepeatedInstructionIsAnsweredFromTheCache) {
  Write(kProgramAddress, {kOpMovAlImm8, 0x11});
  RunAt(kProgramAddress);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x11);

  PokeBehindTheCPUsBack(kProgramAddress, {kOpMovAlImm8, 0x22});
  RunAt(kProgramAddress);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x11);
}

TEST_F(DecodeCacheTest, AReportedWriteDiscardsTheDecode) {
  Write(kProgramAddress, {kOpMovAlImm8, 0x11});
  RunAt(kProgramAddress);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x11);

  Write(kProgramAddress, {kOpMovAlImm8, 0x22});
  RunAt(kProgramAddress);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x22);
}

// The case the page generations exist for. The write is one the CPU makes
// itself, so nothing outside the core has to notice it happened.
TEST_F(DecodeCacheTest, SelfModifyingCodeIsSeen) {
  // MOV BYTE [0x101], 0x22, which overwrites the immediate of the instruction
  // below. Both are in place before anything runs, so that the only thing
  // happening between the two executions of that instruction is the CPU's own
  // store - Write() reports what it writes, and would otherwise be what
  // discarded the decode.
  //
  // The patcher is also placed where it does not land in the same cache entry
  // as its target, so that an eviction cannot stand in for the write.
  const uint32_t patcher = kProgramAddress + 0x21;
  Write(patcher, {0xC6, 0x06, 0x01, 0x01, 0x22});
  Write(kProgramAddress, {kOpMovAlImm8, 0x11});

  RunAt(kProgramAddress);
  ASSERT_EQ(cpu_.registers[kAX] & 0xFF, 0x11);

  RunAt(patcher);
  RunAt(kProgramAddress);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x22);
}

TEST_F(DecodeCacheTest, AWriteToAnotherPageKeepsTheDecode) {
  Write(kProgramAddress, {kOpMovAlImm8, 0x11});
  RunAt(kProgramAddress);

  CPUNotifyMemoryWrite(&cpu_, kProgramAddress + kCodePageSize);
  PokeBehindTheCPUsBack(kProgramAddress, {kOpMovAlImm8, 0x22});
  RunAt(kProgramAddress);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x11);
}

// A page generation is a byte, so 256 writes bring it back to where a decode
// taken before them was recorded. Nothing about the entry says it is stale at
// that point, which is why the wrap discards the whole cache instead.
TEST_F(DecodeCacheTest, AGenerationComingBackRoundDiscardsEverything) {
  Write(kProgramAddress, {kOpMovAlImm8, 0x11});
  RunAt(kProgramAddress);

  for (int i = 0; i < 256; ++i) {
    CPUNotifyMemoryWrite(&cpu_, kProgramAddress);
  }
  PokeBehindTheCPUsBack(kProgramAddress, {kOpMovAlImm8, 0x22});
  RunAt(kProgramAddress);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x22);
}

// An instruction whose later bytes are on the next page is keyed on the page
// its first byte is on, which says nothing about them - so it is not kept.
TEST_F(DecodeCacheTest, AnInstructionStraddlingAPageBoundaryIsNotCached) {
  const uint32_t straddling = kCodePageSize - 1;
  Write(straddling, {kOpMovAlImm8, 0x11});
  RunAt(straddling);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x11);

  PokeBehindTheCPUsBack(straddling, {kOpMovAlImm8, 0x22});
  RunAt(straddling);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x22);
}

// The same argument for the other boundary an instruction can cross. IP wraps
// within the segment where the linear address does not, so the second byte
// here comes from 64KB below the first.
TEST_F(DecodeCacheTest, AnInstructionWrappingTheSegmentIsNotCached) {
  cpu_.registers[kCS] = 0x1000;
  const uint16_t last_offset = 0xFFFF;
  const uint32_t opcode_address = 0x1FFFF;
  const uint32_t immediate_address = 0x10000;
  Write(opcode_address, {kOpMovAlImm8});
  Write(immediate_address, {0x11});
  RunAt(last_offset);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x11);

  PokeBehindTheCPUsBack(immediate_address, {0x22});
  RunAt(last_offset);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x22);
}

// Two addresses a whole cache apart land in the same entry, so the entry has
// to say which of them it holds.
TEST_F(DecodeCacheTest, CollidingAddressesDoNotAliasOntoEachOther) {
  const uint32_t first = kProgramAddress;
  const uint32_t second = kProgramAddress + kNumCacheEntries;
  Write(first, {kOpMovAlImm8, 0x11});
  Write(second, {kOpMovBlImm8, 0x22});

  RunAt(first);
  RunAt(second);
  RunAt(first);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x11);
  EXPECT_EQ(cpu_.registers[kBX] & 0xFF, 0x22);
}

// A hit skips the decode, not the accounting. Both have to come out the same
// or the emulated clock would depend on what happened to be cached.
TEST_F(DecodeCacheTest, AHitAdvancesIPAndChargesCyclesAsTheDecodeDid) {
  Write(kProgramAddress, {kOpMovAlImm8, 0x11});

  RunAt(kProgramAddress);
  const uint16_t ip_after_miss = cpu_.registers[kIP];
  const uint16_t cycles_after_miss = cpu_.cycles_this_tick;

  RunAt(kProgramAddress);
  EXPECT_EQ(cpu_.registers[kIP], ip_after_miss);
  EXPECT_EQ(cpu_.cycles_this_tick, cycles_after_miss);
}

// A decode that fails partway has already written part of an instruction into
// the entry it was decoding into. The entry has to stop claiming to hold
// anything before that, or it goes on offering the address it used to hold
// alongside the wreckage of a different one.
//
// The two instructions here are on different pages so that writing the second
// does not bump the generation of the first, and a whole cache apart so that
// they land in the same entry.
TEST_F(DecodeCacheTest, AFailedDecodeLeavesNothingBehindInItsEntry) {
  // The instruction that gets cached reads through DS, so an ES override left
  // behind in its entry sends it somewhere else and is visible in AL. A
  // failed decode writes the prefix fields and nothing after them, which is
  // exactly what an entry it did not disown would keep.
  cpu_.registers[kES] = 0x0100;
  const uint32_t through_ds = 0x0200;
  const uint32_t through_es = 0x1200;
  Write(through_ds, {0x11});
  Write(through_es, {0x99});

  // MOV AL, [0x0200]
  const uint32_t good = kProgramAddress;
  Write(good, {kOpMovAlMoffs8, through_ds & 0xFF, through_ds >> 8});

  // A run of segment override prefixes one longer than a decode will follow,
  // a whole cache away so that it lands in the same entry, and on another page
  // so that writing it does not discard the entry by itself.
  const uint32_t too_many_prefixes = kProgramAddress + kCodePageSize;
  std::vector<uint8_t> prefixes(kMaxPrefixBytes + 1, kPrefixES);
  prefixes.push_back(kOpIncBx);
  Write(too_many_prefixes, prefixes);

  RunAt(good);
  ASSERT_EQ(cpu_.registers[kAX] & 0xFF, 0x11);

  cpu_.registers[kIP] = too_many_prefixes;
  ASSERT_EQ(CPUTick(&cpu_), kCPUTickInvalid);

  RunAt(good);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x11);
}

TEST_F(DecodeCacheTest, InvalidatingDiscardsEveryDecode) {
  Write(kProgramAddress, {kOpMovAlImm8, 0x11});
  RunAt(kProgramAddress);

  CPUInvalidateDecodeCache(&cpu_);
  PokeBehindTheCPUsBack(kProgramAddress, {kOpMovAlImm8, 0x22});
  RunAt(kProgramAddress);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x22);
}

TEST_F(DecodeCacheTest, WithoutACacheEveryInstructionIsDecoded) {
  CPUSetDecodeCache(&cpu_, nullptr, 0);
  Write(kProgramAddress, {kOpMovAlImm8, 0x11});
  RunAt(kProgramAddress);

  PokeBehindTheCPUsBack(kProgramAddress, {kOpMovAlImm8, 0x22});
  RunAt(kProgramAddress);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x22);
}

// An index is a mask rather than a remainder, so a count that is not a power
// of two cannot be honoured. Taking no cache at all is the safe answer, and
// the only one that cannot silently index past the storage.
TEST_F(DecodeCacheTest, ACountThatIsNotAPowerOfTwoIsRefused) {
  CPUSetDecodeCache(&cpu_, cache_, kNumCacheEntries - 1);
  EXPECT_EQ(cpu_.decode_cache, nullptr);

  Write(kProgramAddress, {kOpMovAlImm8, 0x11});
  RunAt(kProgramAddress);
  PokeBehindTheCPUsBack(kProgramAddress, {kOpMovAlImm8, 0x22});
  RunAt(kProgramAddress);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x22);
}

// A loop is what the cache is for, and its instructions have to come out of it
// in the order they were decoded in.
TEST_F(DecodeCacheTest, ALoopRunsTheSameWayEveryTimeRound) {
  // INC BX; JMP -3
  Write(kProgramAddress, {kOpIncBx, 0xEB, 0xFD});
  cpu_.registers[kIP] = kProgramAddress;
  for (int i = 0; i < 16; ++i) {
    ASSERT_EQ(CPUTick(&cpu_), kCPUTickExecuted);
    ASSERT_EQ(CPUTick(&cpu_), kCPUTickExecuted);
    ASSERT_EQ(cpu_.registers[kIP], kProgramAddress);
    ASSERT_EQ(cpu_.registers[kBX], i + 1);
  }
}

}  // namespace
