// Stack usage: how much of each core's stack has ever been used, for the
// report's stack group. Each stack is painted with a pattern at startup, and
// the most used is how far down from its top the pattern has been written
// over.
//
// The stacks are 4KB each, from the linker script, with core 1's directly
// below core 0's. So core 0 running past the bottom of its own stack writes
// over the frames core 1 is in, which hangs core 1, and with it drawing and
// screenshots, with nothing said. Watching the most used is how that is kept
// from happening.
#ifndef YAX86_PICOCALC_STACK_USAGE_H
#define YAX86_PICOCALC_STACK_USAGE_H

#include <stdint.h>

// Paint both stacks: core 1's whole, and core 0's from its bottom up to a
// little below this function's frame. Call first thing in main(), before
// anything has used more of core 0's stack than main's frame, and before
// core 1 starts.
void StackUsagePaint(void);

// The size of each stack, and the most of core 0's and core 1's that has
// been used.
uint32_t StackUsageSize(void);
uint32_t StackUsageCore0(void);
uint32_t StackUsageCore1(void);

#endif  // YAX86_PICOCALC_STACK_USAGE_H
