#ifndef YAX86_CPU_CYCLES_H
#define YAX86_CPU_CYCLES_H

#ifndef YAX86_IMPLEMENTATION
#include "public.h"
#include "types.h"
#endif  // YAX86_IMPLEMENTATION

enum {
  // What a tick costs while the CPU is halted. It is waiting for an interrupt
  // rather than stopped, so time still passes - and the timer that will wake
  // it is driven from that time.
  kHaltedCycles = 4,
  // Extra cycles a taken jump costs, for the prefetch queue it throws away.
  kJumpTakenCycles = 12,
  // Extra cycles a shift or rotate costs for each bit it moves, when the count
  // comes from CL rather than being 1.
  kShiftCyclesPerBit = 4,
};

#ifndef YAX86_IMPLEMENTATION

// Base execution cost per opcode, excluding the effective address calculation
// and time on the data bus.
extern const uint8_t kOpcodeBaseCycles[256];

// Cycles to compute the effective address of a ModR/M memory operand.
extern uint8_t GetEffectiveAddressCycles(const Instruction* instruction);

// Charge the instruction currently executing for time on the data bus.
extern void AddBusCycles(CPUState* cpu, uint8_t num_bytes);

#endif  // YAX86_IMPLEMENTATION

#endif  // YAX86_CPU_CYCLES_H
