// Tests for the bulk path a repeated MOVS or STOS takes when every byte it
// touches lies inside the window the CPU indexes directly.
//
// The path is gated on a host supplying that window, and CPUTestHelper
// deliberately supplies none - so every other string test in this directory
// exercises the element-at-a-time path, and nothing there reaches these lines
// at all. That is the coverage gap this file exists to close, in the same way
// cycles_test.cpp covers what no architectural test can see.
//
// What almost every test here asserts is that the two paths are
// indistinguishable: the same memory, the same registers, the same flags and
// the same cycle count, for the same program from the same starting state. The
// element path is the definition of what a repeat does, so a case that can tell
// them apart is a bug in the bulk path whichever answer looks more reasonable.

#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "./test_helpers.h"
#include "cpu.h"

using namespace std;

namespace {

// More than a segment, which is what the wrapping cases below need: an offset
// has to be able to wrap the top of its segment while every address involved
// is still inside memory and inside the window, or the run would be declined
// for running out of window and the wrap would never be the reason.
constexpr size_t kMemorySize = 0x20000;  // 128KB

// A segment base clear of both the program and the top of memory, so that a
// run starting near the top of this segment wraps to the bottom of it rather
// than carrying on into the memory above.
constexpr uint16_t kWrapSegment = 0x0100;  // base 0x1000

// Everything an observer outside the CPU could tell the two paths apart by.
struct Outcome {
  vector<uint8_t> memory;
  vector<uint16_t> registers;
  uint16_t flags;
  uint16_t cycles;
};

bool operator==(const Outcome& a, const Outcome& b) {
  return a.memory == b.memory && a.registers == b.registers &&
         a.flags == b.flags && a.cycles == b.cycles;
}

ostream& operator<<(ostream& os, const Outcome& outcome) {
  os << "{ cycles: " << dec << outcome.cycles << ", flags: 0x" << hex
     << outcome.flags << ", SI: 0x" << outcome.registers[kSI] << ", DI: 0x"
     << outcome.registers[kDI] << ", CX: 0x" << outcome.registers[kCX] << " }";
  return os;
}

using SetUp = function<void(CPUTestHelper&)>;

// Runs one instruction with the direct data window either supplied or not, and
// returns everything that could distinguish the two.
Outcome Run(
    const string& name, const string& asm_code, const SetUp& set_up,
    bool with_window) {
  auto helper = CPUTestHelper::CreateWithProgram(name, asm_code, kMemorySize);
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kES] = 0;
  set_up(*helper);
  if (with_window) {
    CPUSetDirectDataWindow(&helper->cpu_, helper->memory_.get(), kMemorySize);
  }
  EXPECT_EQ(CPUTick(&helper->cpu_, 0), kCPUTickExecuted);

  Outcome outcome;
  outcome.memory.assign(
      helper->memory_.get(), helper->memory_.get() + kMemorySize);
  outcome.registers.assign(
      helper->cpu_.registers, helper->cpu_.registers + kNumRegisters);
  outcome.flags = helper->cpu_.flags;
  outcome.cycles = helper->cpu_.cycles_this_tick;
  return outcome;
}

// Runs the same instruction from the same state both ways and asserts nothing
// tells them apart. Returns the bulk outcome, so that a test can go on to say
// what the answer should have been rather than only that the two agree.
Outcome ExpectBothPathsAgree(
    const string& name, const string& asm_code, const SetUp& set_up) {
  const Outcome element = Run(name + "-element", asm_code, set_up, false);
  const Outcome bulk = Run(name + "-bulk", asm_code, set_up, true);
  EXPECT_EQ(element, bulk);
  return bulk;
}

// Fills a range with a recognizable pattern, so that a copy landing one byte
// out is visible rather than plausible.
void FillPattern(CPUTestHelper& helper, uint16_t offset, uint16_t length) {
  for (uint16_t i = 0; i < length; ++i) {
    helper.memory_[offset + i] = static_cast<uint8_t>(0x40 + (i % 0x3B));
  }
}

}  // namespace

class StringBulkTest : public ::testing::Test {};

// The ordinary case in both widths and both directions: the bulk path and the
// element path produce the same memory, registers and cycle count.
TEST_F(StringBulkTest, AgreesWithTheElementPathForEveryShapeOfRun) {
  struct Case {
    const char* name;
    const char* asm_code;
    bool backwards;
    uint16_t count;
  };
  const Case cases[] = {
      {"movsb-forward", "rep movsb\n", false, 64},
      {"movsw-forward", "rep movsw\n", false, 64},
      {"movsb-backward", "rep movsb\n", true, 64},
      {"movsw-backward", "rep movsw\n", true, 64},
      {"stosb-forward", "rep stosb\n", false, 64},
      {"stosw-forward", "rep stosw\n", false, 64},
      {"stosb-backward", "rep stosb\n", true, 64},
      {"stosw-backward", "rep stosw\n", true, 64},
      // A single element still goes through the bulk path, and is where an
      // off-by-one in the addressability test would show up.
      {"movsb-one", "rep movsb\n", false, 1},
      {"movsw-one-backward", "rep movsw\n", true, 1},
  };
  for (const Case& test_case : cases) {
    SCOPED_TRACE(test_case.name);
    const uint16_t count = test_case.count;
    const bool backwards = test_case.backwards;
    ExpectBothPathsAgree(
        test_case.name, test_case.asm_code,
        [count, backwards](CPUTestHelper& helper) {
          FillPattern(helper, 0x0400, 0x200);
          helper.cpu_.registers[kAX] = 0x1234;
          helper.cpu_.registers[kCX] = count;
          // Backwards runs start at the top of the same ranges, so that both
          // directions cover the same bytes.
          helper.cpu_.registers[kSI] = backwards ? 0x05FE : 0x0400;
          helper.cpu_.registers[kDI] = backwards ? 0x07FE : 0x0600;
          CPUSetFlag(&helper.cpu_, kDF, backwards);
        });
  }
}

// A copy whose two ends overlap has to reproduce what moving one element at a
// time does, which for a forward copy is to propagate the first element
// through the whole range rather than to move a block. A bulk path that copies
// the range in one go, or in the wrong order, gets this wrong.
TEST_F(StringBulkTest, OverlappingCopiesPropagateAsTheyWouldOneAtATime) {
  const Outcome outcome = ExpectBothPathsAgree(
      "movsb-overlap-forward", "rep movsb\n", [](CPUTestHelper& helper) {
        helper.memory_[0x0400] = 0xAB;
        for (uint16_t i = 1; i < 16; ++i) {
          helper.memory_[0x0400 + i] = 0x00;
        }
        helper.cpu_.registers[kCX] = 15;
        helper.cpu_.registers[kSI] = 0x0400;
        helper.cpu_.registers[kDI] = 0x0401;
        CPUSetFlag(&helper.cpu_, kDF, false);
      });
  // Every byte of the range is the one the copy started from.
  for (uint16_t i = 0; i < 16; ++i) {
    EXPECT_EQ(outcome.memory[0x0400 + i], 0xAB) << "at offset " << i;
  }
}

// The same for words, and at the one offset where it bites: with the
// destination one byte above the source, the element path reads both bytes of
// an element before writing either, so the high byte it reads is the one that
// was there before the element before it was written.
TEST_F(StringBulkTest, OverlappingWordCopiesReadBothBytesBeforeWritingEither) {
  ExpectBothPathsAgree(
      "movsw-overlap-by-one", "rep movsw\n", [](CPUTestHelper& helper) {
        FillPattern(helper, 0x0400, 64);
        helper.cpu_.registers[kCX] = 16;
        helper.cpu_.registers[kSI] = 0x0400;
        helper.cpu_.registers[kDI] = 0x0401;
        CPUSetFlag(&helper.cpu_, kDF, false);
      });
}

// A repeat with CX already zero moves nothing, costs nothing beyond the
// instruction itself, and leaves SI and DI alone.
TEST_F(StringBulkTest, ARepeatOfZeroElementsDoesNothing) {
  const Outcome outcome = ExpectBothPathsAgree(
      "movsb-zero", "rep movsb\n", [](CPUTestHelper& helper) {
        FillPattern(helper, 0x0400, 16);
        helper.cpu_.registers[kCX] = 0;
        helper.cpu_.registers[kSI] = 0x0400;
        helper.cpu_.registers[kDI] = 0x0500;
        CPUSetFlag(&helper.cpu_, kDF, false);
      });
  EXPECT_EQ(outcome.registers[kSI], 0x0400);
  EXPECT_EQ(outcome.registers[kDI], 0x0500);
  EXPECT_EQ(outcome.memory[0x0500], 0x00);
}

// A run that would leave the window has to go back to the element path for all
// of it, since the part above the window is only reachable through the host's
// callback - and indexing the window past its end would not merely miss the
// device, it would write over whatever the host keeps behind guest RAM.
//
// Nothing else in this file can tell a run that respected the bound from one
// that ignored it, because everywhere else the window and the memory behind it
// are the same array. This host is shaped the way a real machine is instead:
// conventional RAM the CPU indexes, and something above it - video memory, a
// ROM - that a write has to be handed to the host to reach.
namespace {

class SplitMemoryHost {
 public:
  enum : uint32_t {
    // What the CPU may index. The RAM behind it is never addressed through the
    // window, so a byte found up there came from a run that went past the end.
    kWindowEnd = 0x1000,
    kRamSize = 0x2000,
    kDeviceSize = 0x1000,
  };

  SplitMemoryHost() {
    cpu_ = CPUState{};
    cpu_.config.context = this;
    cpu_.config.read_memory_byte = TestReadMemoryByte;
    cpu_.config.write_memory_byte = TestWriteMemoryByte;
    CPUInit(&cpu_);
    CPUSetDirectDataWindow(&cpu_, ram_, kWindowEnd);
  }

  // Where a linear address lives. Below the window it is RAM; above it, for as
  // far as this host goes, it is the device.
  uint8_t& At(uint32_t address) {
    if (address < kWindowEnd) {
      return ram_[address];
    }
    if (address < kWindowEnd + kDeviceSize) {
      return device_[address - kWindowEnd];
    }
    return nowhere_;
  }

  static uint8_t TestReadMemoryByte(CPUState* cpu, uint32_t address) {
    return static_cast<SplitMemoryHost*>(cpu->config.context)->At(address);
  }
  static void TestWriteMemoryByte(
      CPUState* cpu, uint32_t address, uint8_t value) {
    static_cast<SplitMemoryHost*>(cpu->config.context)->At(address) = value;
  }

  CPUState cpu_;
  uint8_t ram_[kRamSize] = {};
  uint8_t device_[kDeviceSize] = {};
  uint8_t nowhere_ = 0;
};

}  // namespace

TEST_F(StringBulkTest, ARunLeavingTheWindowReachesTheDeviceAboveIt) {
  SplitMemoryHost host;
  // REP STOSB, fetched through the host's callback like any other byte.
  host.ram_[0x10] = 0xF3;
  host.ram_[0x11] = 0xAA;
  host.cpu_.registers[kCS] = 0;
  host.cpu_.registers[kIP] = 0x0010;
  host.cpu_.registers[kES] = 0;
  host.cpu_.registers[kAX] = 0x00CD;
  host.cpu_.registers[kCX] = 16;
  host.cpu_.registers[kDI] = SplitMemoryHost::kWindowEnd - 8;
  CPUSetFlag(&host.cpu_, kDF, false);
  ASSERT_EQ(CPUTick(&host.cpu_, 0), kCPUTickExecuted);

  for (uint32_t i = 0; i < 8; ++i) {
    // Eight bytes inside the window, and eight that had to be handed over.
    EXPECT_EQ(host.ram_[SplitMemoryHost::kWindowEnd - 8 + i], 0xCD) << i;
    EXPECT_EQ(host.device_[i], 0xCD) << i;
    // Which is where a run that indexed straight past the end would have put
    // them instead.
    EXPECT_EQ(host.ram_[SplitMemoryHost::kWindowEnd + i], 0x00) << i;
  }
  EXPECT_EQ(host.cpu_.registers[kCX], 0);
  EXPECT_EQ(host.cpu_.registers[kDI], SplitMemoryHost::kWindowEnd + 8);
}

// A run whose offset would wrap the top of its segment cannot be a straight
// run over memory: the element path recomputes the address from the wrapped
// 16-bit offset, so the bytes after the wrap land at the bottom of the same
// segment rather than in the paragraph above it. The bulk path has to decline,
// and these two are what say it does.
//
// Both put the whole run inside the window, so running out of window cannot be
// what declines them and the wrap has to be.
TEST_F(StringBulkTest, ARunWrappingTheTopOfItsSegmentFallsBack) {
  const Outcome outcome = ExpectBothPathsAgree(
      "stosb-segment-wrap", "rep stosb\n", [](CPUTestHelper& helper) {
        helper.cpu_.registers[kES] = kWrapSegment;
        helper.cpu_.registers[kAX] = 0x00CD;
        helper.cpu_.registers[kCX] = 8;
        helper.cpu_.registers[kDI] = 0xFFFC;
        CPUSetFlag(&helper.cpu_, kDF, false);
      });
  // Four bytes at the top of the segment, and four that wrapped to the bottom
  // of it - not the eight consecutive bytes a straight run would have left.
  for (uint16_t i = 0; i < 4; ++i) {
    EXPECT_EQ(outcome.memory[0x10FFC + i], 0xCD) << "at offset " << i;
    EXPECT_EQ(outcome.memory[0x1000 + i], 0xCD) << "at offset " << i;
    EXPECT_EQ(outcome.memory[0x11000 + i], 0x00) << "at offset " << i;
  }
  EXPECT_EQ(outcome.registers[kDI], 0x0004);
}

// A backwards run stepping below offset 0 wraps the same way, to the top of
// the same segment.
TEST_F(StringBulkTest, ABackwardRunWrappingBelowZeroFallsBackToo) {
  const Outcome outcome = ExpectBothPathsAgree(
      "stosb-wrap-below-zero", "rep stosb\n", [](CPUTestHelper& helper) {
        helper.cpu_.registers[kES] = kWrapSegment;
        helper.cpu_.registers[kAX] = 0x00CD;
        helper.cpu_.registers[kCX] = 8;
        helper.cpu_.registers[kDI] = 0x0003;
        CPUSetFlag(&helper.cpu_, kDF, true);
      });
  for (uint16_t i = 0; i < 4; ++i) {
    EXPECT_EQ(outcome.memory[0x1000 + i], 0xCD) << "at offset " << i;
    EXPECT_EQ(outcome.memory[0x10FFC + i], 0xCD) << "at offset " << i;
    // Below the segment base is where a run that stepped down without wrapping
    // would have gone.
    EXPECT_EQ(outcome.memory[0x0FFC + i], 0x00) << "at offset " << i;
  }
  EXPECT_EQ(outcome.registers[kDI], 0xFFFB);
}

// A segment override moves where a MOVS reads from, and the bulk path has to
// apply it exactly as the element path does - it is the one part of the
// address that is not fixed by the instruction.
TEST_F(StringBulkTest, ASegmentOverrideMovesWhereTheCopyReadsFrom) {
  const Outcome outcome = ExpectBothPathsAgree(
      "movsb-es-override", "rep es movsb\n", [](CPUTestHelper& helper) {
        // ES is a paragraph above DS, so the override moves the source up by
        // 16 bytes. Both ranges are filled so that reading the wrong one is
        // not a read of zeroes.
        helper.cpu_.registers[kES] = 0x0001;
        FillPattern(helper, 0x0400, 0x80);
        helper.cpu_.registers[kCX] = 16;
        helper.cpu_.registers[kSI] = 0x0400;
        // The destination is ES:DI, which the override does not touch.
        helper.cpu_.registers[kDI] = 0x0600;
        CPUSetFlag(&helper.cpu_, kDF, false);
      });
  // Read from ES:SI = 0x0410, written to ES:DI = 0x0610.
  for (uint16_t i = 0; i < 16; ++i) {
    EXPECT_EQ(outcome.memory[0x0610 + i], outcome.memory[0x0410 + i])
        << "at offset " << i;
  }
}

// A repeat long enough to run the 16-bit cycle counter past the top of its
// range has to wrap where adding one access at a time would wrap. Summing the
// whole run first and adding once is only the same arithmetic because both
// wrap modulo the same thing, and this is what says so.
TEST_F(StringBulkTest, ALongRunChargesWhatOneElementAtATimeWouldHave) {
  // 0x1000 words read and written is 0x8000 bus cycles, which is past 16 bits
  // once the base cost is added to it.
  const Outcome outcome = ExpectBothPathsAgree(
      "movsw-long", "rep movsw\n", [](CPUTestHelper& helper) {
        helper.cpu_.registers[kCX] = 0x1000;
        helper.cpu_.registers[kSI] = 0x0000;
        helper.cpu_.registers[kDI] = 0x2000;
        CPUSetFlag(&helper.cpu_, kDF, false);
      });
  EXPECT_EQ(outcome.registers[kCX], 0);
  EXPECT_EQ(outcome.registers[kSI], 0x2000);
  EXPECT_EQ(outcome.registers[kDI], 0x4000);
}

// A repeat that writes over code the CPU has already decoded has to discard
// those decodes, exactly as a write through the element path would. The bulk
// path reports one write per page rather than one per byte, so this is where
// a page it forgot to report would show up.
TEST_F(StringBulkTest, ACopyOverCachedCodeDiscardsTheDecodes) {
  // The program loops back to the same INC AX until the copy overwrites it
  // with INC BX. Running it once fills the decode cache from the page the
  // copy is about to land on.
  auto helper = CPUTestHelper::CreateWithProgram(
      "movs-over-cached-code",
      // The copy source holds one INC BX.
      "  mov word [0x0800], 0x43\n"
      "target:\n"
      "  inc ax\n"
      "  rep movsb\n"
      "  jmp target\n",
      kMemorySize);
  helper->cpu_.registers[kDS] = 0;
  helper->cpu_.registers[kES] = 0;
  CPUDecodeCacheEntry decode_cache[16] = {};
  helper->cpu_.config.decode_cache = decode_cache;
  helper->cpu_.config.decode_cache_num_entries = 16;
  // Only the flags and the cache fields, so the loaded program and where CS:IP
  // points at it survive this.
  CPUInit(&helper->cpu_);
  CPUSetDirectDataWindow(&helper->cpu_, helper->memory_.get(), kMemorySize);

  // The copy replaces the INC AX at "target" with the INC BX at 0x0800.
  const uint16_t target = kCOMFileLoadOffset + 6;
  ASSERT_EQ(helper->memory_[target], 0x40);  // INC AX
  helper->cpu_.registers[kCX] = 1;
  helper->cpu_.registers[kSI] = 0x0800;
  helper->cpu_.registers[kDI] = target;
  CPUSetFlag(&helper->cpu_, kDF, false);

  // MOV, INC AX, REP MOVSB, JMP, and then the instruction at "target" again -
  // which is now INC BX, and has to be decoded afresh rather than served from
  // the cache.
  for (int i = 0; i < 5; ++i) {
    ASSERT_EQ(CPUTick(&helper->cpu_, 0), kCPUTickExecuted) << "tick " << i;
  }
  EXPECT_EQ(helper->memory_[target], 0x43);  // INC BX
  EXPECT_EQ(helper->cpu_.registers[kAX], 1);
  EXPECT_EQ(helper->cpu_.registers[kBX], 1);
}
