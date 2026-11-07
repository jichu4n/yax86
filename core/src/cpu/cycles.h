#ifndef Y_CPU_CYCLES_H
#define Y_CPU_CYCLES_H

#ifndef Y_IMPLEMENTATION
#include "../util/common.h"
#include "public.h"
#include "types.h"
#endif  // Y_IMPLEMENTATION

enum {
  // What a tick costs while the CPU is halted. It is waiting for an interrupt
  // rather than stopped, so time still passes - and the timer that will wake
  // it is driven from that time.
  Y_CPU_HALTED_CYCLES = 4,
  // Extra cycles a taken jump costs, for the prefetch queue it throws away.
  Y_CPU_JUMP_TAKEN_CYCLES = 12,
  // Extra cycles a shift or rotate costs for each bit it moves, when the count
  // comes from CL rather than being 1.
  Y_CPU_SHIFT_CYCLES_PER_BIT = 4,
  // Cycles per byte transferred over the data bus. The 8088's bus is 8 bits
  // wide, so a word costs twice a byte.
  Y_CPU_BUS_CYCLES_PER_BYTE = 4,
};

// Cycles to compute the effective address of a ModR/M memory operand.
Y_MODULE_PRIVATE uint8_t
YCPUGetEffectiveAddressCycles(const YCPUInstruction* instruction);

// Charge the instruction currently executing for time on the data bus.
Y_MODULE_PRIVATE void YCPUAddBusCycles(YCPUState* cpu, uint8_t num_bytes);

#endif  // Y_CPU_CYCLES_H
