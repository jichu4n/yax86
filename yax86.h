// 8086 CPU emulator.
#ifndef YAX86_H
#define YAX86_H

#include <stdint.h>

// CPU registers
typedef struct {
  // General-purpose registers
  uint16_t AX;  // Accumulator Register
  uint16_t BX;  // Base Register
  uint16_t CX;  // Counter Register
  uint16_t DX;  // Data Register
  // Segment registers
  uint16_t CS;  // Code Segment Register
  uint16_t DS;  // Data Segment Register
  uint16_t SS;  // Stack Segment Register
  uint16_t ES;  // Extra Segment Register
  // Pointer and index registers
  uint16_t SP;  // Stack Pointer Register
  uint16_t BP;  // Base Pointer Register
  uint16_t SI;  // Source Index Register
  uint16_t DI;  // Destination Index Register
  uint16_t IP;  // Instruction Pointer Register
} CPURegisters;
#define NUM_REGISTERS 8

// CPU flags
typedef struct {
  // Status flags
  uint8_t CF : 1;  // Carry Flag
  uint8_t PF : 1;  // Parity Flag
  uint8_t AF : 1;  // Auxiliary Carry Flag
  uint8_t ZF : 1;  // Zero Flag
  uint8_t SF : 1;  // Sign Flag
  uint8_t OF : 1;  // Overflow Flag
  // Control flags
  uint8_t TF : 1;  // Trap Flag
  uint8_t IF : 1;  // Interrupt Enable Flag
  uint8_t DF : 1;  // Direction Flag
} CPUFlags;

// Data size for memory and register operations
typedef enum { kByte = 1, kWord = 2 } CPUDataWidth;

// Runtime configuration
typedef struct {
  // Required - hooks to read and write memory.
  uint8_t (*read_memory)(uint16_t address, CPUDataWidth width);
  void (*write_memory)(uint16_t address, CPUDataWidth width, uint8_t value);
} CPUConfig;

// CPU state
typedef struct {
  // Pointer to the configuration
  CPUConfig* config;

  // Register values
  CPURegisters registers;
  // Flag values
  CPUFlags flags;
} CPUState;

#endif  // YAX86_H
