#include <cstring>
#include <vector>

#include "gtest/gtest.h"
#include "platform.h"

namespace {

enum : uint8_t {
  kOpHlt = 0xF4,
  // MOV AL, imm8
  kOpMovAlImm8 = 0xB0,
  // MOV BL, imm8
  kOpMovBlImm8 = 0xB3,
  // MOV AL, [moffs16]
  kOpMovAlMoffs = 0xA0,
  // MOV [moffs16], AL
  kOpMovMoffsAl = 0xA2,
};

constexpr uint32_t kPhysicalSize = 64 * 1024;
constexpr uint32_t kPagedSize = 64 * 1024;
constexpr uint8_t kPageShift = 9;
constexpr uint32_t kPageSize = 1 << kPageShift;
constexpr uint32_t kNumPages = kPagedSize >> kPageShift;
constexpr uint32_t kMaxFrames = 4;
// The paged region starts here, at segment 0x1000.
constexpr uint32_t kPagedStart = kPhysicalSize;
constexpr uint16_t kPagedSegment = kPagedStart >> 4;

// A host as the PicoCalc's is: a few frames, the pages in a backing store of
// their own, a frame given to the page longest in one when none is free, and a
// page written back only once written.
class PlatformPagedMemoryTest : public ::testing::Test {
 protected:
  void SetUp() override {
    for (uint32_t i = 0; i < kPagedSize; ++i) {
      backing_[i] = static_cast<uint8_t>(i * 7 + (i >> 8));
    }
  }

  bool Init(
      uint32_t num_frames = kMaxFrames, uint32_t paged_size = kPagedSize) {
    num_frames_ = num_frames;
    paged_.context = this;
    paged_.size = paged_size;
    paged_.page_shift = kPageShift;
    paged_.pages = pages_;
    paged_.frames = frames_;
    paged_.page_fault = PageFault;
    platform_.config.physical_memory_size = kPhysicalSize;
    platform_.config.physical_memory = ram_;
    platform_.config.paged_memory = &paged_;
    platform_.config.vram = vram_;
    if (!YPlatformInit(&platform_)) {
      return false;
    }
    platform_.cpu.registers[Y_CPU_SS] = 0;
    platform_.cpu.registers[Y_CPU_SP] = 0xFFFE;
    return true;
  }

  static void PageFault(
      YPlatformPagedMemory* memory, uint32_t page, bool write) {
    auto* self = static_cast<PlatformPagedMemoryTest*>(memory->context);
    self->faults_.push_back({page, write});
    if (self->fail_faults_) {
      return;
    }
    uint16_t& entry = self->pages_[page];
    if (!(entry & Y_PLATFORM_PAGED_MEMORY_RESIDENT)) {
      uint32_t frame;
      if (self->frames_used_ < self->num_frames_) {
        frame = self->frames_used_++;
      } else {
        frame = self->next_victim_;
        self->next_victim_ = (self->next_victim_ + 1) % self->num_frames_;
        self->Evict(self->frame_page_[frame]);
      }
      std::memcpy(
          &self->frames_[frame * kPageSize], &self->backing_[page * kPageSize],
          kPageSize);
      self->frame_page_[frame] = page;
      entry = static_cast<uint16_t>(frame | Y_PLATFORM_PAGED_MEMORY_RESIDENT);
    }
    if (write) {
      entry |= Y_PLATFORM_PAGED_MEMORY_WRITABLE;
    }
  }

  void Evict(uint32_t page) {
    const uint16_t entry = pages_[page];
    const uint32_t frame = entry & Y_PLATFORM_PAGED_MEMORY_FRAME_MASK;
    if (entry & Y_PLATFORM_PAGED_MEMORY_WRITABLE) {
      std::memcpy(
          &backing_[page * kPageSize], &frames_[frame * kPageSize], kPageSize);
    }
    // Scribbled over, so that a read of the frame for this page gets it wrong.
    std::memset(&frames_[frame * kPageSize], 0xCC, kPageSize);
    pages_[page] = 0;
  }

  void Load(uint32_t address, const std::vector<uint8_t>& bytes) {
    for (size_t i = 0; i < bytes.size(); ++i) {
      YPlatformWriteMemoryByte(&platform_, address + i, bytes[i]);
    }
  }

  void RunUntilHalt() {
    for (int i = 0; i < 100; ++i) {
      if (YPlatformTick(&platform_) != Y_PLATFORM_RUNNING ||
          platform_.cpu.is_halted) {
        return;
      }
    }
  }

  uint8_t al() const { return platform_.cpu.registers[Y_CPU_AX] & 0xFF; }
  uint8_t bl() const { return platform_.cpu.registers[Y_CPU_BX] & 0xFF; }

  struct Fault {
    uint32_t page;
    bool write;
  };

  YPlatformState platform_ = {};
  YPlatformPagedMemory paged_ = {};
  uint8_t ram_[kPhysicalSize] = {0};
  uint8_t vram_[Y_VIDEO_CGA_VRAM_SIZE] = {0};
  uint16_t pages_[kNumPages] = {0};
  uint8_t frames_[kMaxFrames * kPageSize] = {0};
  uint8_t backing_[kPagedSize] = {0};
  uint32_t frame_page_[kMaxFrames] = {0};
  uint32_t num_frames_ = kMaxFrames;
  uint32_t frames_used_ = 0;
  uint32_t next_victim_ = 0;
  bool fail_faults_ = false;
  std::vector<Fault> faults_;
};

TEST_F(PlatformPagedMemoryTest, EveryByteReadsBackThroughFarFewerFrames) {
  ASSERT_TRUE(Init());
  for (uint32_t i = 0; i < kPagedSize; ++i) {
    YPlatformWriteMemoryByte(
        &platform_, kPagedStart + i, static_cast<uint8_t>(i ^ (i >> 9)));
  }
  for (uint32_t i = 0; i < kPagedSize; ++i) {
    ASSERT_EQ(
        YPlatformReadMemoryByte(&platform_, kPagedStart + i),
        static_cast<uint8_t>(i ^ (i >> 9)))
        << i;
  }
}

TEST_F(PlatformPagedMemoryTest, PhysicalMemoryIsNotPaged) {
  ASSERT_TRUE(Init());
  YPlatformWriteMemoryByte(&platform_, kPhysicalSize - 1, 0x5A);
  EXPECT_EQ(ram_[kPhysicalSize - 1], 0x5A);
  EXPECT_TRUE(faults_.empty());
  // The CPU still indexes it directly.
  EXPECT_EQ(platform_.cpu.direct_data_window.end, kPhysicalSize);
}

TEST_F(PlatformPagedMemoryTest, AReadHandsThePageOutReadOnly) {
  ASSERT_TRUE(Init());
  EXPECT_EQ(YPlatformReadMemoryByte(&platform_, kPagedStart + 5), backing_[5]);
  ASSERT_EQ(faults_.size(), 1u);
  EXPECT_EQ(faults_[0].page, 0u);
  EXPECT_FALSE(faults_[0].write);
  // A second read of the page is the frame's.
  YPlatformReadMemoryByte(&platform_, kPagedStart + 6);
  EXPECT_EQ(faults_.size(), 1u);
  // The first write is asked about, so the host knows to write it back.
  YPlatformWriteMemoryByte(&platform_, kPagedStart + 7, 0x11);
  ASSERT_EQ(faults_.size(), 2u);
  EXPECT_EQ(faults_[1].page, 0u);
  EXPECT_TRUE(faults_[1].write);
  // And the next is not.
  YPlatformWriteMemoryByte(&platform_, kPagedStart + 8, 0x22);
  EXPECT_EQ(faults_.size(), 2u);
}

TEST_F(PlatformPagedMemoryTest, ClearingResidentAsksAboutTheNextRead) {
  ASSERT_TRUE(Init());
  YPlatformReadMemoryByte(&platform_, kPagedStart);
  pages_[0] &= static_cast<uint16_t>(~Y_PLATFORM_PAGED_MEMORY_RESIDENT);
  EXPECT_EQ(YPlatformReadMemoryByte(&platform_, kPagedStart), backing_[0]);
  ASSERT_EQ(faults_.size(), 2u);
  EXPECT_EQ(faults_[1].page, 0u);
}

TEST_F(PlatformPagedMemoryTest, AFaultLeftUnansweredReadsOpenBus) {
  ASSERT_TRUE(Init());
  fail_faults_ = true;
  EXPECT_EQ(YPlatformReadMemoryByte(&platform_, kPagedStart + 1), 0xFF);
  YPlatformWriteMemoryByte(&platform_, kPagedStart + 1, 0x00);
  EXPECT_EQ(faults_.size(), 2u);
  fail_faults_ = false;
  EXPECT_EQ(YPlatformReadMemoryByte(&platform_, kPagedStart + 1), backing_[1]);
}

TEST_F(PlatformPagedMemoryTest, RunsCodeAcrossAPageBoundary) {
  ASSERT_TRUE(Init());
  // MOV AL, 0x42 split across pages 0 and 1, then MOV BL, 0x24.
  Load(
      kPagedStart + kPageSize - 1,
      {kOpMovAlImm8, 0x42, kOpMovBlImm8, 0x24, kOpHlt});
  platform_.cpu.registers[Y_CPU_CS] = kPagedSegment;
  platform_.cpu.registers[Y_CPU_IP] = kPageSize - 1;
  RunUntilHalt();
  EXPECT_EQ(al(), 0x42);
  EXPECT_EQ(bl(), 0x24);
}

// One frame, shared by the code and the data it reads. Reading the data takes
// the code's frame, and the next fetch must not take the data's bytes for the
// code's: the window instruction fetch kept into the frame has to go.
TEST_F(PlatformPagedMemoryTest, AFrameTakenFromCodeIsNotFetchedFrom) {
  ASSERT_TRUE(Init(1));
  // MOV AL, [0x0400] reads page 2; MOV BL, 0x24; HLT.
  Load(kPagedStart, {kOpMovAlMoffs, 0x00, 0x04, kOpMovBlImm8, 0x24, kOpHlt});
  Load(kPagedStart + 0x400, {0x99});
  platform_.cpu.registers[Y_CPU_CS] = kPagedSegment;
  platform_.cpu.registers[Y_CPU_DS] = kPagedSegment;
  platform_.cpu.registers[Y_CPU_IP] = 0;
  RunUntilHalt();
  EXPECT_EQ(al(), 0x99);
  EXPECT_EQ(bl(), 0x24);
  EXPECT_TRUE(platform_.cpu.is_halted);
}

// The same, with the code writing: its page is written back on the way out
// and the write is there when it comes back.
TEST_F(PlatformPagedMemoryTest, AWriteSurvivesItsPageLeavingItsFrame) {
  ASSERT_TRUE(Init(1));
  // MOV AL, 0x77; MOV [0x0400], AL; MOV AL, [0x0001]; HLT - the last reads
  // back page 0's second byte, the 0x77 the first instruction was assembled
  // with.
  Load(
      kPagedStart, {kOpMovAlImm8, 0x77, kOpMovMoffsAl, 0x00, 0x04,
                    kOpMovAlMoffs, 0x01, 0x00, kOpHlt});
  platform_.cpu.registers[Y_CPU_CS] = kPagedSegment;
  platform_.cpu.registers[Y_CPU_DS] = kPagedSegment;
  platform_.cpu.registers[Y_CPU_IP] = 0;
  RunUntilHalt();
  EXPECT_EQ(al(), 0x77);
  EXPECT_EQ(YPlatformReadMemoryByte(&platform_, kPagedStart + 0x400), 0x77);
  EXPECT_TRUE(platform_.cpu.is_halted);
}

// The CPU indexes a page its entry allows without asking, and asks about one
// it does not: a read of a page with RESIDENT cleared, and a write to one
// without WRITABLE.
TEST_F(PlatformPagedMemoryTest, TheCPUAsksOnlyWhatTheEntryDoesNotAllow) {
  ASSERT_TRUE(Init());
  // MOV AL, [0x0400]; MOV AL, [0x0401]; MOV [0x0402], AL; MOV [0x0403], AL;
  // HLT, in physical memory, on page 2 of paged memory.
  Load(
      0x100, {kOpMovAlMoffs, 0x00, 0x04, kOpMovAlMoffs, 0x01, 0x04,
              kOpMovMoffsAl, 0x02, 0x04, kOpMovMoffsAl, 0x03, 0x04, kOpHlt});
  platform_.cpu.registers[Y_CPU_CS] = 0;
  platform_.cpu.registers[Y_CPU_DS] = kPagedSegment;
  platform_.cpu.registers[Y_CPU_IP] = 0x100;
  RunUntilHalt();
  ASSERT_EQ(faults_.size(), 2u);
  EXPECT_EQ(faults_[0].page, 2u);
  EXPECT_FALSE(faults_[0].write);
  EXPECT_EQ(faults_[1].page, 2u);
  EXPECT_TRUE(faults_[1].write);
  EXPECT_EQ(
      YPlatformReadMemoryByte(&platform_, kPagedStart + 0x403),
      backing_[0x401]);

  pages_[2] &= static_cast<uint16_t>(
      ~(Y_PLATFORM_PAGED_MEMORY_RESIDENT | Y_PLATFORM_PAGED_MEMORY_WRITABLE));
  platform_.cpu.registers[Y_CPU_IP] = 0x100;
  platform_.cpu.is_halted = false;
  RunUntilHalt();
  EXPECT_EQ(faults_.size(), 4u);
}

TEST_F(PlatformPagedMemoryTest, AllOfConventionalMemoryMayBePaged) {
  platform_.config.physical_memory_size = 0;
  paged_.context = this;
  paged_.size = kPagedSize;
  paged_.page_shift = kPageShift;
  paged_.pages = pages_;
  paged_.frames = frames_;
  paged_.page_fault = PageFault;
  platform_.config.paged_memory = &paged_;
  platform_.config.vram = vram_;
  ASSERT_TRUE(YPlatformInit(&platform_));
  EXPECT_EQ(platform_.cpu.direct_data_window.end, 0u);
  YPlatformWriteMemoryByte(&platform_, 0x10, 0x3C);
  EXPECT_EQ(YPlatformReadMemoryByte(&platform_, 0x10), 0x3C);
  EXPECT_EQ(faults_.size(), 1u);
}

TEST_F(PlatformPagedMemoryTest, InitRefusesPagesThatDoNotFit) {
  EXPECT_FALSE(Init(kMaxFrames, kPagedSize - 1));
}

TEST_F(PlatformPagedMemoryTest, InitRefusesPagesLargerThanTheMemoryMaps) {
  paged_.page_shift = Y_PLATFORM_MAX_PAGED_MEMORY_PAGE_SHIFT + 1;
  paged_.context = this;
  paged_.size = kPagedSize;
  paged_.pages = pages_;
  paged_.frames = frames_;
  paged_.page_fault = PageFault;
  platform_.config.physical_memory_size = kPhysicalSize;
  platform_.config.physical_memory = ram_;
  platform_.config.paged_memory = &paged_;
  platform_.config.vram = vram_;
  EXPECT_FALSE(YPlatformInit(&platform_));
}

TEST_F(PlatformPagedMemoryTest, InitRefusesPhysicalMemoryEndingMidPage) {
  paged_.context = this;
  paged_.size = kPagedSize;
  paged_.page_shift = kPageShift;
  paged_.pages = pages_;
  paged_.frames = frames_;
  paged_.page_fault = PageFault;
  platform_.config.physical_memory_size = kPhysicalSize - kPageSize;
  platform_.config.physical_memory = ram_;
  platform_.config.paged_memory = &paged_;
  platform_.config.vram = vram_;
  EXPECT_FALSE(YPlatformInit(&platform_));
}

TEST_F(PlatformPagedMemoryTest, InitRefusesMoreThan640KB) {
  paged_.context = this;
  paged_.size = Y_PLATFORM_MAX_PHYSICAL_MEMORY_SIZE;
  paged_.page_shift = kPageShift;
  paged_.pages = pages_;
  paged_.frames = frames_;
  paged_.page_fault = PageFault;
  platform_.config.physical_memory_size = kPhysicalSize;
  platform_.config.physical_memory = ram_;
  platform_.config.paged_memory = &paged_;
  platform_.config.vram = vram_;
  EXPECT_FALSE(YPlatformInit(&platform_));
}

}  // namespace
