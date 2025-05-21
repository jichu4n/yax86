#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "../yax86.h"

// COM file load offset.
constexpr uint16_t kCOMFileLoadOffset = 0x100;

// Overload the << operator for EncodedInstruction to print its contents.
std::ostream& operator<<(
    std::ostream& os, const EncodedInstruction& instruction);

// Returns the name of a CPU flag for debugging.
std::string GetFlagName(Flag flag);

// Test helper to assemble instructions using fasm and return the machine code.
std::vector<uint8_t> Assemble(
    const std::string& name, const std::string& asm_code);

// Default memory size for the CPU test helper.
constexpr size_t kDefaultMemorySize = 0x1000;  // 4KB

// Test helper to manage CPU state and memory.
class CPUTestHelper {
 public:
  explicit CPUTestHelper(size_t memory_size = kDefaultMemorySize);
  ~CPUTestHelper();

  // Create a test helper and load compiled assembly code into memory.
  static std::unique_ptr<CPUTestHelper> CreateWithProgram(
      const std::string& name, const std::string& asm_code,
      size_t memory_size = kDefaultMemorySize);

  // Load data into memory.
  void Load(const std::vector<uint8_t>& data, uint16_t offset);

  // Load a COM file into memory at 0x100, and set up CS and IP pointers to
  // point to the start of the code.
  void LoadCOM(const std::vector<uint8_t>& code);

  // Assemble instructions using FASM to a COM binary and load it into memory.
  // Returns the size of the loaded code.
  size_t AssembleAndLoadProgram(
      const std::string& name, const std::string& asm_code);

  // Execute instructions from CS:IP.
  void ExecuteInstructions(int num_instructions);

  // Helper function to test the value of a list of flags.
  void CheckFlags(const std::vector<std::pair<Flag, bool> >& flags);

  // Main memory size.
  const size_t memory_size_;
  // Main memory.
  const std::unique_ptr<uint8_t[]> memory_;
  // CPU state.
  CPUState cpu_;

 private:
  struct Context {
    uint8_t* memory;
    size_t memory_size;
  } context_;
  CPUConfig config_;
};

#endif  // TEST_HELPERS_H
