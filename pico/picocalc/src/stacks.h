// The two cores' stacks, 4KB each, from the linker script: core 1's directly
// below core 0's, so that core 0 running past the bottom of its own writes
// over the frames core 1 is in, which hangs core 1 with nothing said. Each is
// painted at startup, and the report gives how much of it has ever been
// written over.
#ifndef YAX86_PICOCALC_STACKS_H
#define YAX86_PICOCALC_STACKS_H

#include <stdint.h>

// Paint core 0's stack from its bottom to a little below this function's
// frame, and core 1's, before core 1 starts on it, whole. Call first, before
// anything has used more of the stack than main's frame, or core 1 has
// started.
void StacksPaint(void);

// The size of each stack, and the most of core 0's and core 1's that has
// been used.
uint32_t StacksSize(void);
uint32_t StacksCore0Used(void);
uint32_t StacksCore1Used(void);

#endif  // YAX86_PICOCALC_STACKS_H
