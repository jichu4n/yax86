// Minimal harness to demo and debug 8086 CPU emulation.
//
// Implements a bare minimum set of DOS interrupts that maps to standard input /
// output.

#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#define YAX86_IMPLEMENTATION
#include "cpu.h"

using namespace std;

// VM memory.
uint8_t memory[0x2000] = {0};

vector<uint8_t> Assemble(const string& asm_file_name) {
  // Assemble the code using fasm to a COM file
  string com_file_name = asm_file_name + ".com";
  string command = "fasm " + asm_file_name + " " + com_file_name;
  if (system(command.c_str()) != 0) {
    throw runtime_error("Failed to run command: " + command);
  }

  // Read the COM file into memory
  ifstream com_file(com_file_name, ios::binary);
  if (!com_file) {
    throw runtime_error("Failed to read COM file: " + com_file_name);
  }
  vector<uint8_t> machine_code(
      (istreambuf_iterator<char>(com_file)), istreambuf_iterator<char>());
  com_file.close();

  return machine_code;
}

// Interrupt handler for the demo.
ExecuteStatus HandleInterrupt(CPUState* cpu, uint8_t interrupt_number) {
  // cout << "Handling interrupt: " << hex << (int)interrupt_number
  //      << "DX = " << hex << cpu->registers[kDX] << dec << endl;
  if (interrupt_number != 0x21) {
    return kExecuteUnhandledInterrupt;
  }

  uint8_t ah = (cpu->registers[kAX] >> 8) & 0xFF;
  uint16_t dx = cpu->registers[kDX];
  switch (ah) {
    case 0x01: {  // Read character
      char ch;
      cin.get(ch);
      uint8_t ah = (cpu->registers[kAX] >> 8) & 0xFF;
      cpu->registers[kAX] = (ah << 8) | static_cast<uint8_t>(ch);
      return kExecuteSuccess;
    }
    case 0x02: {  // Print character
      cout << static_cast<char>(dx & 0xFF) << flush;
      return kExecuteSuccess;
    }
    case 0x09: {  // Print string
      for (size_t i = dx; memory[i] != '$'; ++i) {
        cout << static_cast<char>(memory[i]);
      }
      cout << flush;
      return kExecuteSuccess;
    }
    case 0x0A: {  // Read string
      uint16_t dx = cpu->registers[kDX];
      uint8_t max_length = memory[dx];
      string input;
      getline(cin, input);
      if (input.size() > max_length - 1) {
        input.resize(max_length - 1);
      }
      memory[dx + 1] = input.size();
      input += '\n';
      memcpy(memory + dx + 2, input.c_str(), input.size());
      return kExecuteSuccess;
    }
    case 0x4C:  // Terminate program
      return kExecuteHalt;
    case 0x2C: {  // Get system time
      struct timeval tv;
      gettimeofday(&tv, nullptr);
      struct tm* info = localtime(&tv.tv_sec);
      uint8_t ch = info->tm_hour;
      uint8_t cl = info->tm_min;
      uint8_t dh = info->tm_sec;
      uint8_t dl = tv.tv_usec / 10000;
      cpu->registers[kCX] = (ch << 8) | cl;
      cpu->registers[kDX] = (dh << 8) | dl;
      return kExecuteSuccess;
    }
    default:
      cerr << "Unhandled DOS interrupt: " << hex
           << static_cast<int>(interrupt_number) << " AH = " << hex
           << static_cast<int>(ah) << endl;
      return kExecuteHalt;
  }
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    cerr << "Usage: " << argv[0] << " <assembly_program>" << endl;
    return EXIT_FAILURE;
  }

  CPUConfig config = {0};
  config.on_before_execute_instruction =
      [](CPUState* cpu, Instruction* instruction) -> ExecuteStatus {
    // cout << "Executing instruction at " << hex
    //      << ((cpu->registers[kCS] << 4) + cpu->registers[kIP]) << " : "
    //      << static_cast<int>(instruction->opcode) << endl;
    // sleep(1);
    return kExecuteSuccess;
  };
  config.read_memory_byte = [](CPUState* cpu, uint32_t address) -> uint8_t {
    if (address >= sizeof(memory)) {
      cerr << "Memory read out of bounds at address: " << hex << address
           << endl;
      throw runtime_error("Memory read out of bounds");
    }
    return memory[address];
  };
  config.write_memory_byte = [](CPUState* cpu, uint32_t address,
                                uint8_t value) {
    if (address >= sizeof(memory)) {
      cerr << "Memory write out of bounds at address: " << hex << address
           << endl;
      throw runtime_error("Memory write out of bounds");
    }
    memory[address] = value;
  };
  config.handle_interrupt = HandleInterrupt;

  // Initialize CPU state
  CPUState cpu;
  InitCPU(&cpu);
  cpu.config = &config;

  // Load the assembly program into memory
  auto machine_code = Assemble(argv[1]);
  memcpy(memory + 0x100, machine_code.data(), machine_code.size());

  // Set CS:IP to the start of the program
  cpu.registers[kCS] = 0;
  cpu.registers[kIP] = 0x100;
  // Set stack pointer to the top of the stack
  cpu.registers[kSP] = sizeof(memory);

  // Execute the program!
  ExecuteStatus status = RunMainLoop(&cpu);
  if (status != kExecuteSuccess && status != kExecuteHalt) {
    cerr << "Program execution failed with status: " << status << endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
