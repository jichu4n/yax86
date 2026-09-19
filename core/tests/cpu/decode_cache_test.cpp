#include <string>
#include <vector>

#include "cpu.h"
#include "gtest/gtest.h"

namespace {

// Two 64KB segments, so that a segment wrap has somewhere to wrap to.
constexpr uint32_t kMemorySize = 0x20000;
// Small enough that two addresses can be made to collide on purpose.
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
// reused; the new one means it was not. Poking memory behind the CPU's back is
// exactly what a host must not do, which is what makes it the lever here.
class DecodeCacheTest : public ::testing::Test {
 protected:
  void SetUp() override { Init(cache_, kNumCacheEntries); }

  // The cache is part of the config, so choosing a different one means
  // initializing again.
  void Init(CPUDecodeCacheEntry* entries, uint32_t num_entries) {
    memory_.assign(kMemorySize, 0);
    cpu_ = CPUState{};
    cpu_.config.context = this;
    cpu_.config.read_memory_byte = ReadMemoryByte;
    cpu_.config.write_memory_byte = WriteMemoryByte;
    cpu_.config.get_instruction_fetch_window = GetInstructionFetchWindow;
    cpu_.config.decode_cache = entries;
    cpu_.config.decode_cache_num_entries = num_entries;
    CPUInit(&cpu_);
    CPUSetDirectDataWindow(&cpu_, memory_.data(), kMemorySize);
    cpu_.registers[kCS] = 0;
    cpu_.registers[kDS] = 0;
    cpu_.registers[kSS] = 0;
    cpu_.registers[kSP] = 0xFFFE;
    cpu_.registers[kIP] = kProgramAddress;
  }

  // Writes the way a host is supposed to, so anything decoded from those bytes
  // is discarded.
  void Write(uint32_t address, const std::vector<uint8_t>& bytes) {
    for (size_t i = 0; i < bytes.size(); ++i) {
      CPUNotifyMemoryWrite(&cpu_, address + i);
      memory_[address + i] = bytes[i];
    }
  }

  // Writes without telling the CPU, which is what makes a reused decode
  // visible.
  void PokeBehindTheCPUsBack(
      uint32_t address, const std::vector<uint8_t>& bytes) {
    for (size_t i = 0; i < bytes.size(); ++i) {
      memory_[address + i] = bytes[i];
    }
  }

  void RunAt(uint16_t ip) {
    cpu_.registers[kIP] = ip;
    ASSERT_EQ(CPUTick(&cpu_, 0), kCPUTickExecuted);
  }

  static uint8_t ReadMemoryByte(CPUState* cpu, uint32_t address) {
    DecodeCacheTest* self = static_cast<DecodeCacheTest*>(cpu->config.context);
    return address < kMemorySize ? self->memory_[address] : 0xFF;
  }

  static void WriteMemoryByte(CPUState* cpu, uint32_t address, uint8_t value) {
    DecodeCacheTest* self = static_cast<DecodeCacheTest*>(cpu->config.context);
    if (address < kMemorySize) {
      self->memory_[address] = value;
    }
  }

  static void RecordLogLine(
      void* context, const LogModule* module, LogLevel level, uint64_t tick,
      const char* message, size_t length) {
    (void)module;
    (void)level;
    (void)tick;
    static_cast<DecodeCacheTest*>(context)->log_lines_.push_back(
        std::string(message, length));
  }

  static void GetInstructionFetchWindow(CPUState* cpu, uint32_t address) {
    DecodeCacheTest* self = static_cast<DecodeCacheTest*>(cpu->config.context);
    if (address >= kMemorySize) {
      cpu->instruction_fetch_window.data = nullptr;
      return;
    }
    cpu->instruction_fetch_window.data = self->memory_.data();
    cpu->instruction_fetch_window.start = 0;
    cpu->instruction_fetch_window.end = kMemorySize;
  }

  std::vector<std::string> log_lines_;
  std::vector<uint8_t> memory_;
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

// The case the page generations exist for: the write is one the CPU makes
// itself, so nothing outside the core has to notice it happened.
TEST_F(DecodeCacheTest, SelfModifyingCodeIsSeen) {
  // MOV BYTE [0x101], 0x22, which overwrites the immediate of the instruction
  // below. Both are written before anything runs, so the CPU's own store is
  // the only thing happening between the two executions of that instruction -
  // Write() reports what it writes and would otherwise be what discarded the
  // decode. The patcher also lands in a different entry than its target, so an
  // eviction cannot stand in for the write.
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

// A page generation is a byte, so 256 writes bring it back to what a decode
// taken before them recorded. Nothing about the entry says it is stale at that
// point, which is why the wrap discards the whole cache instead.
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

// An instruction is keyed on the page its first byte is on, which says nothing
// about later bytes on the next page - so it is not kept.
TEST_F(DecodeCacheTest, AnInstructionStraddlingAPageBoundaryIsNotCached) {
  const uint32_t straddling = kCodePageSize - 1;
  Write(straddling, {kOpMovAlImm8, 0x11});
  RunAt(straddling);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x11);

  PokeBehindTheCPUsBack(straddling, {kOpMovAlImm8, 0x22});
  RunAt(straddling);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x22);
}

// The other boundary an instruction can cross. IP wraps within the segment
// where the linear address does not, so the second byte here comes from 64KB
// below the first.
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

// Two addresses a whole cache apart land in the same entry.
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

// A hit skips the decode, not the accounting - otherwise the emulated clock
// would depend on what happened to be cached.
TEST_F(DecodeCacheTest, AHitAdvancesIPAndChargesCyclesAsTheDecodeDid) {
  Write(kProgramAddress, {kOpMovAlImm8, 0x11});

  RunAt(kProgramAddress);
  const uint16_t ip_after_miss = cpu_.registers[kIP];
  const uint16_t cycles_after_miss = cpu_.cycles_this_tick;

  RunAt(kProgramAddress);
  EXPECT_EQ(cpu_.registers[kIP], ip_after_miss);
  EXPECT_EQ(cpu_.cycles_this_tick, cycles_after_miss);
}

// A failed decode has already written part of an instruction into its entry,
// so the entry must disown its contents first - otherwise it goes on offering
// the address it used to hold alongside the wreckage of a different one. The
// two instructions are on different pages so that writing one does not bump
// the other's generation, and a whole cache apart so that they collide.
TEST_F(DecodeCacheTest, AFailedDecodeLeavesNothingBehindInItsEntry) {
  // The cached instruction reads through DS, so an ES override left behind in
  // its entry is visible in AL. The prefix fields are exactly what a failed
  // decode writes and an undisowned entry would keep.
  cpu_.registers[kES] = 0x0100;
  const uint32_t through_ds = 0x0200;
  const uint32_t through_es = 0x1200;
  Write(through_ds, {0x11});
  Write(through_es, {0x99});

  // MOV AL, [0x0200]
  const uint32_t good = kProgramAddress;
  Write(good, {kOpMovAlMoffs8, through_ds & 0xFF, through_ds >> 8});

  // One prefix longer than a decode will follow.
  const uint32_t too_many_prefixes = kProgramAddress + kCodePageSize;
  std::vector<uint8_t> prefixes(kMaxPrefixBytes + 1, kPrefixES);
  prefixes.push_back(kOpIncBx);
  Write(too_many_prefixes, prefixes);

  RunAt(good);
  ASSERT_EQ(cpu_.registers[kAX] & 0xFF, 0x11);

  cpu_.registers[kIP] = too_many_prefixes;
  ASSERT_EQ(CPUTick(&cpu_, 0), kCPUTickInvalid);

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
  Init(nullptr, 0);
  Write(kProgramAddress, {kOpMovAlImm8, 0x11});
  RunAt(kProgramAddress);

  PokeBehindTheCPUsBack(kProgramAddress, {kOpMovAlImm8, 0x22});
  RunAt(kProgramAddress);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x22);
}

// An index is a mask rather than a remainder, so a count that is not a power
// of two cannot be honoured. Running without a cache is the safe answer, and
// the only one that cannot silently index past the storage.
TEST_F(DecodeCacheTest, ACountThatIsNotAPowerOfTwoIsRefused) {
  Init(cache_, kNumCacheEntries - 1);
  EXPECT_EQ(cpu_.config.decode_cache, nullptr);

  Write(kProgramAddress, {kOpMovAlImm8, 0x11});
  RunAt(kProgramAddress);
  PokeBehindTheCPUsBack(kProgramAddress, {kOpMovAlImm8, 0x22});
  RunAt(kProgramAddress);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x22);
}

// One entry is a legal power of two: every address maps to it, so it holds the
// last instruction decoded and nothing else.
TEST_F(DecodeCacheTest, ACountOfOneIsAccepted) {
  Init(cache_, 1);
  ASSERT_EQ(cpu_.config.decode_cache, cache_);

  Write(kProgramAddress, {kOpMovAlImm8, 0x11});
  RunAt(kProgramAddress);
  PokeBehindTheCPUsBack(kProgramAddress, {kOpMovAlImm8, 0x22});
  RunAt(kProgramAddress);
  EXPECT_EQ(cpu_.registers[kAX] & 0xFF, 0x11);
}

// A host about to run 10% slower than it asked to should be told why.
TEST_F(DecodeCacheTest, ABadCountIsLogged) {
  LoggerConfig logger_config = {};
  logger_config.context = this;
  logger_config.write_line = RecordLogLine;
  logger_config.enabled_modules = 0xFFFFFFFF;
  logger_config.min_level = kLogLevelError;
  Logger logger;
  LoggerInit(&logger, &logger_config);

  // The logger has to be in place before the init that rejects the count:
  // CPUInit() clears its own copy of the pointer on the way out, so a second
  // call sees no cache and has nothing to complain about.
  log_lines_.clear();
  cpu_ = CPUState{};
  cpu_.config.logger = &logger;
  cpu_.config.decode_cache = cache_;
  cpu_.config.decode_cache_num_entries = kNumCacheEntries - 1;
  CPUInit(&cpu_);

  EXPECT_EQ(log_lines_.size(), 1u);
  EXPECT_EQ(cpu_.config.decode_cache, nullptr);
}

// A loop is what the cache is for.
TEST_F(DecodeCacheTest, ALoopRunsTheSameWayEveryTimeRound) {
  // INC BX; JMP -3
  Write(kProgramAddress, {kOpIncBx, 0xEB, 0xFD});
  cpu_.registers[kIP] = kProgramAddress;
  for (int i = 0; i < 16; ++i) {
    ASSERT_EQ(CPUTick(&cpu_, 0), kCPUTickExecuted);
    ASSERT_EQ(CPUTick(&cpu_, 0), kCPUTickExecuted);
    ASSERT_EQ(cpu_.registers[kIP], kProgramAddress);
    ASSERT_EQ(cpu_.registers[kBX], i + 1);
  }
}

}  // namespace
