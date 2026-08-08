// Runs the 8088 hardware-generated CPU test suite against the emulator.
//
//   https://github.com/SingleStepTests/8088
//
// Each test gives a starting register, flag and memory state plus the bytes of
// one instruction, and the state a real 8088 was left in afterwards. Only the
// architectural result is checked here - the per-cycle bus and prefetch queue
// records in the suite are for cycle-accurate emulators and are skipped.
//
// Test data is not in the repository. Fetch it once, after which
// tools/run-tests.sh picks these up along with everything else:
//
//   ./tools/download-cpu-hardware-tests.sh
//
// Everything here is expected to pass apart from the divide instructions - see
// KnownDivergence below.

#include <gtest/gtest.h>
#include <zlib.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "cpu.h"

namespace {

// Physical address space the tests address.
constexpr size_t kMemorySize = 1024 * 1024;

// Registers in the order the suite's register bitmask uses them, mapped onto
// this emulator's own ordering. The flags register is last and is not in
// RegisterIndex, so it gets a sentinel.
constexpr int kFlagsPseudoRegister = -1;
constexpr int kMooRegisterOrder[] = {
    kAX, kBX, kCX, kDX, kCS, kSS, kDS,
    kES, kSP, kBP, kSI, kDI, kIP, kFlagsPseudoRegister,
};
constexpr int kNumMooRegisters =
    sizeof(kMooRegisterOrder) / sizeof(kMooRegisterOrder[0]);

// A register or flags value from a test's initial or final state.
struct RegisterValues {
  // Indexed by position in kMooRegisterOrder.
  bool present[kNumMooRegisters] = {false};
  uint16_t value[kNumMooRegisters] = {0};
};

struct CPUStateSnapshot {
  RegisterValues registers;
  // Physical address to byte value.
  std::map<uint32_t, uint8_t> ram;
};

struct MooTest {
  // Whether this test was parsed into something checkable. A parse that
  // silently yielded nothing would make every test pass vacuously.
  //
  // Only the initial state is required. A final state records just the values
  // that changed, and legitimately holds none at all - a taken "jz -2" jumps
  // to itself and leaves every register as it was.
  bool is_parsed() const {
    if (bytes.empty()) {
      return false;
    }
    for (int i = 0; i < kNumMooRegisters; ++i) {
      if (initial.registers.present[i]) {
        return true;
      }
    }
    return false;
  }

  std::string name;
  std::vector<uint8_t> bytes;
  CPUStateSnapshot initial;
  // Only the values that changed are recorded.
  CPUStateSnapshot final_state;
};

// ============================================================================
// MOO binary format
// ============================================================================
//
// A file is a sequence of chunks, each a four character type, a 32-bit
// little-endian length, and that many bytes of payload. A "MOO " header chunk
// is followed by one "TEST" chunk per test, whose payload is itself a sequence
// of chunks.

uint32_t ReadU32(const uint8_t* p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

uint16_t ReadU16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

// Walks the chunks in a buffer, calling visit(type, data, length) for each.
template <typename Visitor>
bool ForEachChunk(const uint8_t* data, size_t size, Visitor visit) {
  size_t pos = 0;
  while (pos + 8 <= size) {
    char type[5] = {0};
    memcpy(type, data + pos, 4);
    const uint32_t length = ReadU32(data + pos + 4);
    pos += 8;
    if (pos + length > size) {
      return false;
    }
    visit(std::string(type), data + pos, length);
    pos += length;
  }
  return true;
}

RegisterValues ParseRegisters(const uint8_t* data, uint32_t length) {
  RegisterValues registers;
  if (length < 2) {
    return registers;
  }
  const uint16_t bitmask = ReadU16(data);
  const uint8_t* p = data + 2;
  uint32_t remaining = length - 2;
  for (int i = 0; i < kNumMooRegisters; ++i) {
    if (!((bitmask >> i) & 1)) {
      continue;
    }
    if (remaining < 2) {
      break;
    }
    registers.present[i] = true;
    registers.value[i] = ReadU16(p);
    p += 2;
    remaining -= 2;
  }
  return registers;
}

std::map<uint32_t, uint8_t> ParseRam(const uint8_t* data, uint32_t length) {
  std::map<uint32_t, uint8_t> ram;
  if (length < 4) {
    return ram;
  }
  const uint32_t count = ReadU32(data);
  if (length < 4 + count * 5) {
    return ram;
  }
  const uint8_t* p = data + 4;
  for (uint32_t i = 0; i < count; ++i) {
    ram[ReadU32(p)] = p[4];
    p += 5;
  }
  return ram;
}

CPUStateSnapshot ParseCPUState(const uint8_t* data, uint32_t length) {
  CPUStateSnapshot state;
  ForEachChunk(
      data, length,
      [&state](const std::string& type, const uint8_t* p, uint32_t len) {
        if (type == "REGS") {
          state.registers = ParseRegisters(p, len);
        } else if (type == "RAM ") {
          state.ram = ParseRam(p, len);
        }
        // QUEU is the prefetch queue, which this emulator does not model.
      });
  return state;
}

// Decompresses a .MOO.gz file into memory. The suite ships compressed, and the
// files expand to several times their download size, so they are read as-is
// rather than being unpacked to disk.
bool ReadGzipFile(const std::string& path, std::vector<uint8_t>* out) {
  gzFile file = gzopen(path.c_str(), "rb");
  if (file == nullptr) {
    return false;
  }
  uint8_t buffer[64 * 1024];
  int bytes_read;
  while ((bytes_read = gzread(file, buffer, sizeof(buffer))) > 0) {
    out->insert(out->end(), buffer, buffer + bytes_read);
  }
  const bool ok = bytes_read == 0;
  gzclose(file);
  return ok;
}

// Reads every test out of a .MOO.gz file. Returns false if it is unusable.
bool LoadMooFile(const std::string& path, std::vector<MooTest>* tests) {
  std::vector<uint8_t> data;
  if (!ReadGzipFile(path, &data) || data.empty()) {
    return false;
  }

  return ForEachChunk(
      data.data(), data.size(),
      [tests](const std::string& type, const uint8_t* p, uint32_t len) {
        if (type != "TEST") {
          return;
        }
        // A TEST chunk's payload starts with a 32-bit index, then its
        // subchunks.
        if (len < 4) {
          return;
        }
        MooTest test;
        ForEachChunk(
            p + 4, len - 4,
            [&test](const std::string& sub, const uint8_t* q, uint32_t sublen) {
              if (sub == "NAME" && sublen >= 4) {
                const uint32_t n = ReadU32(q);
                if (sublen >= 4 + n) {
                  test.name.assign((const char*)q + 4, n);
                }
              } else if (sub == "BYTS" && sublen >= 4) {
                const uint32_t n = ReadU32(q);
                if (sublen >= 4 + n) {
                  test.bytes.assign(q + 4, q + 4 + n);
                }
              } else if (sub == "INIT") {
                test.initial = ParseCPUState(q, sublen);
              } else if (sub == "FINA") {
                test.final_state = ParseCPUState(q, sublen);
              }
              // CYCL and HASH are not used.
            });
        tests->push_back(test);
      });
}

// ============================================================================
// Running a test
// ============================================================================

uint8_t g_memory[kMemorySize];

uint8_t ReadMemoryByte(YAX86_UNUSED CPUState* cpu, uint32_t address) {
  return address < kMemorySize ? g_memory[address] : 0xFF;
}

void WriteMemoryByte(
    YAX86_UNUSED CPUState* cpu, uint32_t address, uint8_t value) {
  if (address < kMemorySize) {
    g_memory[address] = value;
  }
}

// Reads the flag masks emitted by tools/download-cpu-hardware-tests.sh. Bits
// clear in a mask are left undefined by the 8088 for that opcode, and are not
// compared.
std::map<std::string, uint16_t> LoadFlagMasks(const std::string& dir) {
  std::map<std::string, uint16_t> masks;
  std::ifstream file(dir + "/flags_masks.txt");
  std::string opcode;
  std::string mask;
  while (file >> opcode >> mask) {
    masks[opcode] = (uint16_t)strtoul(mask.c_str(), nullptr, 16);
  }
  return masks;
}

// Formats a register mismatch for the failure message.
const char* MooRegisterName(int index) {
  static const char* kNames[] = {"AX", "BX", "CX", "DX", "CS", "SS", "DS",
                                 "ES", "SP", "BP", "SI", "DI", "IP", "FLAGS"};
  return kNames[index];
}

std::string DescribeBytes(const std::vector<uint8_t>& bytes) {
  std::string out;
  char buffer[8];
  for (uint8_t byte : bytes) {
    snprintf(buffer, sizeof(buffer), "%02X ", byte);
    out += buffer;
  }
  return out;
}

// ============================================================================
// Known divergences
// ============================================================================
//
// Differences from the 8088 that this emulator deliberately does not
// reproduce, because doing so would mean emulating the divide microcode step
// by step and nothing an IBM PC/XT runs can tell the difference.
//
// These are matched narrowly rather than by skipping the opcode outright, so
// that everything else those opcodes do is still checked. A divide that
// produced the wrong quotient or remainder, or failed to raise a divide error,
// still fails here.
enum KnownDivergence {
  kNoKnownDivergence = 0,

  // The flags a divide leaves behind when it raises a divide error.
  //
  // The 8086/8088 divides one bit at a time and updates the arithmetic flags
  // on every pass, so a divide error leaves the last pass's flags behind. They
  // are documented as undefined and the suite masks them out of its register
  // comparison - but the interrupt pushes them, so they reappear as the flags
  // word on the stack, where nothing masks them.
  kDivideErrorFlags = 1 << 0,

  // The sign IDIV gives the quotient.
  //
  // For some operands the 8088 negates the quotient when it should not, or the
  // other way about, leaving a result that does not agree with its own
  // remainder. Real software cannot depend on an answer that is arithmetically
  // wrong.
  kIdivByteQuotientSign = 1 << 1,
  kIdivWordQuotientSign = 1 << 2,
};

// What each opcode is allowed to diverge on, and how many mismatches that is
// expected to let through. The counts are asserted, so that a change which
// diverges more fails here rather than quietly printing a larger number.
//
// They are tied to the pinned revision of the test data - see
// tools/download-cpu-hardware-tests.sh - and to a whole opcode file, which is
// all or nothing, so downloading a subset does not disturb them.
struct OpcodeDivergences {
  const char* opcode;
  unsigned kinds;
  int expected_count;
};

const OpcodeDivergences kKnownDivergences[] = {
    {"F6.6", kDivideErrorFlags, 7694},                           // DIV r/m8
    {"F7.6", kDivideErrorFlags, 7485},                           // DIV r/m16
    {"F6.7", kDivideErrorFlags | kIdivByteQuotientSign, 11243},  // IDIV r/m8
    {"F7.7", kDivideErrorFlags | kIdivWordQuotientSign, 11275},  // IDIV r/m16
};

const OpcodeDivergences* FindKnownDivergences(const std::string& opcode) {
  for (const OpcodeDivergences& entry : kKnownDivergences) {
    if (opcode == entry.opcode) {
      return &entry;
    }
  }
  return nullptr;
}

uint32_t ToLinearAddress(uint16_t segment, uint16_t offset) {
  return ((((uint32_t)segment) << 4) + offset) & 0xFFFFF;
}

// Whether two values of AX differ only in the sign of the quotient a divide
// left in it. A byte divide puts the quotient in AL and the remainder in AH; a
// word divide puts the whole quotient in AX.
bool IsQuotientSignDivergence(
    unsigned divergences, uint16_t expected, uint16_t actual) {
  if (divergences & kIdivByteQuotientSign) {
    return (expected >> 8) == (actual >> 8) &&
           (uint8_t)(expected + actual) == 0;
  }
  if (divergences & kIdivWordQuotientSign) {
    return (uint16_t)(expected + actual) == 0;
  }
  return false;
}

// Runs one test, returning an empty string on success or a description of the
// first mismatch. Mismatches that are a known divergence are counted in
// *num_divergences and are not treated as failures.
std::string RunMooTest(
    const MooTest& test, uint16_t flags_mask, unsigned divergences,
    int* num_divergences) {
  memset(g_memory, 0, sizeof(g_memory));

  CPUConfig config = {0};
  config.read_memory_byte = ReadMemoryByte;
  config.write_memory_byte = WriteMemoryByte;
  CPUState cpu;
  CPUInit(&cpu, &config);

  // The expected end state is the starting state with the recorded changes
  // applied, so that a register or byte the instruction should not have
  // touched is checked as well.
  RegisterValues expected = test.initial.registers;
  for (int i = 0; i < kNumMooRegisters; ++i) {
    if (test.final_state.registers.present[i]) {
      expected.present[i] = true;
      expected.value[i] = test.final_state.registers.value[i];
    }
  }
  std::map<uint32_t, uint8_t> expected_ram = test.initial.ram;
  for (const auto& entry : test.final_state.ram) {
    expected_ram[entry.first] = entry.second;
  }

  for (int i = 0; i < kNumMooRegisters; ++i) {
    if (!test.initial.registers.present[i]) {
      continue;
    }
    const int reg = kMooRegisterOrder[i];
    if (reg == kFlagsPseudoRegister) {
      cpu.flags = test.initial.registers.value[i];
    } else {
      cpu.registers[reg] = test.initial.registers.value[i];
    }
  }
  for (const auto& entry : test.initial.ram) {
    if (entry.first < kMemorySize) {
      g_memory[entry.first] = entry.second;
    }
  }

  CPUTick(&cpu);

  char message[512];
  for (int i = 0; i < kNumMooRegisters; ++i) {
    if (!expected.present[i]) {
      continue;
    }
    const int reg = kMooRegisterOrder[i];
    uint16_t actual_value;
    uint16_t expected_value = expected.value[i];
    if (reg == kFlagsPseudoRegister) {
      actual_value = cpu.flags & flags_mask;
      expected_value &= flags_mask;
    } else {
      actual_value = cpu.registers[reg];
    }
    if (actual_value != expected_value) {
      if (reg == kAX &&
          IsQuotientSignDivergence(divergences, expected_value, actual_value)) {
        ++*num_divergences;
        continue;
      }
      snprintf(
          message, sizeof(message),
          "%s: %s expected %04X, got %04X  [%s] (initial flags %04X, IP %04X "
          "-> %04X, CS %04X)",
          test.name.c_str(), MooRegisterName(i), expected_value, actual_value,
          DescribeBytes(test.bytes).c_str(), test.initial.registers.value[13],
          test.initial.registers.value[12], cpu.registers[kIP],
          cpu.registers[kCS]);
      return message;
    }
  }
  // A divide error pushes flags, CS and IP, so afterwards the flags word sits
  // two words above where the stack pointer came to rest.
  const uint32_t pushed_flags_low =
      ToLinearAddress(cpu.registers[kSS], (uint16_t)(cpu.registers[kSP] + 4));
  const uint32_t pushed_flags_high =
      ToLinearAddress(cpu.registers[kSS], (uint16_t)(cpu.registers[kSP] + 5));
  for (const auto& entry : expected_ram) {
    if (entry.first >= kMemorySize) {
      continue;
    }
    if (g_memory[entry.first] != entry.second) {
      if ((divergences & kDivideErrorFlags) &&
          (entry.first == pushed_flags_low ||
           entry.first == pushed_flags_high)) {
        ++*num_divergences;
        continue;
      }
      snprintf(
          message, sizeof(message),
          "%s: memory %05X expected %02X, got %02X  [%s]", test.name.c_str(),
          entry.first, entry.second, g_memory[entry.first],
          DescribeBytes(test.bytes).c_str());
      return message;
    }
  }
  return "";
}

// Opcode name from a test data file path, e.g. ".../8D.MOO" -> "8D".
std::string OpcodeFromPath(const std::string& path) {
  const size_t slash = path.find_last_of('/');
  const std::string base =
      slash == std::string::npos ? path : path.substr(slash + 1);
  const size_t dot = base.rfind(".MOO.gz");
  return dot == std::string::npos ? base : base.substr(0, dot);
}

class CPUHardware : public ::testing::TestWithParam<std::string> {};

// There are no instances unless opcode data has been downloaded, which is the
// normal state of a fresh checkout and of CI.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CPUHardware);

TEST_P(CPUHardware, Matches8088) {
  const std::string path = GetParam();
  const std::string opcode = OpcodeFromPath(path);

  std::vector<MooTest> tests;
  ASSERT_TRUE(LoadMooFile(path, &tests)) << "could not read " << path;
  ASSERT_FALSE(tests.empty()) << "no tests in " << path;
  // Guard against a parse that finds tests but no state to compare, which
  // would make every one of them pass without checking anything.
  for (const MooTest& test : tests) {
    ASSERT_TRUE(test.is_parsed())
        << "test '" << test.name << "' in " << path
        << " has no instruction bytes or initial register state - the file "
           "format has probably changed";
  }

  const auto masks = LoadFlagMasks(YAX86_CPU_TEST_DATA_DIR);
  const auto mask_entry = masks.find(opcode);
  const uint16_t flags_mask =
      mask_entry == masks.end() ? 0xFFFF : mask_entry->second;

  const OpcodeDivergences* known = FindKnownDivergences(opcode);
  const unsigned divergences = known ? known->kinds : kNoKnownDivergence;

  int failures = 0;
  int num_divergences = 0;
  std::string first_failure;
  for (const MooTest& test : tests) {
    const std::string failure =
        RunMooTest(test, flags_mask, divergences, &num_divergences);
    if (!failure.empty()) {
      if (failures == 0) {
        first_failure = failure;
      }
      ++failures;
    }
  }

  EXPECT_EQ(failures, 0) << opcode << ": " << failures << " of " << tests.size()
                         << " tests failed. First: " << first_failure;
  if (failures == 0) {
    // A real failure returns before the rest of a test is compared, which
    // would drag the divergence count down with it - so only hold the count to
    // its expected value once everything else agrees.
    const int expected_divergences = known ? known->expected_count : 0;
    EXPECT_EQ(num_divergences, expected_divergences)
        << opcode << " diverged from the 8088 " << num_divergences
        << " times, expected " << expected_divergences
        << ". A larger number means something newly disagrees with the "
           "hardware; a smaller one means a divergence has been fixed and the "
           "expected count in kKnownDivergences should come down.";

    std::cout << "  opcode " << opcode << ": " << tests.size()
              << " tests passed";
    if (num_divergences > 0) {
      std::cout << " (" << num_divergences
                << " known divergences from the 8088 ignored)";
    }
    std::cout << std::endl;
  }
}

// Enumerates the opcode files that have been downloaded, if any.
std::vector<std::string> DownloadedTestFiles() {
  std::vector<std::string> paths;
  // Not using a directory scan, to keep this portable: the download script
  // records what it fetched.
  std::ifstream index(std::string(YAX86_CPU_TEST_DATA_DIR) + "/downloaded.txt");
  std::string line;
  while (std::getline(index, line)) {
    if (!line.empty()) {
      paths.push_back(std::string(YAX86_CPU_TEST_DATA_DIR) + "/" + line);
    }
  }
  return paths;
}

INSTANTIATE_TEST_SUITE_P(
    Opcodes, CPUHardware, ::testing::ValuesIn(DownloadedTestFiles()),
    [](const ::testing::TestParamInfo<std::string>& info) {
      // Group opcodes are named like "D0.4", and a test name may only
      // contain alphanumerics and underscores.
      std::string name = "Opcode" + OpcodeFromPath(info.param);
      for (char& c : name) {
        if (!isalnum(static_cast<unsigned char>(c))) {
          c = '_';
        }
      }
      return name;
    });

}  // namespace
