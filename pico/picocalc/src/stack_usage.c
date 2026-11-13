#include "stack_usage.h"

#include "pico/stdlib.h"

extern uint32_t __StackBottom;
extern uint32_t __StackTop;
extern uint32_t __StackOneBottom;
extern uint32_t __StackOneTop;

enum {
  // What the stacks are painted with. Any value does, as long as a stack is
  // unlikely to hold it.
  kStackPaint = 0x5AC4DEAD,
  // What painting core 0's stack leaves of it below the painter's own frame.
  kStackPaintMargin = 64,
};

__noinline void StackUsagePaint(void) {
  volatile uint32_t here = 0;
  uint32_t* const end = (uint32_t*)((uintptr_t)&here - kStackPaintMargin) - 1;
  for (uint32_t* word = &__StackBottom; word < end; ++word) {
    *word = kStackPaint;
  }
  for (uint32_t* word = &__StackOneBottom; word < &__StackOneTop; ++word) {
    *word = kStackPaint;
  }
}

// The most of a stack that has been used: from its top down to the lowest
// word no longer as painted.
static uint32_t StackUsed(const uint32_t* bottom, const uint32_t* top) {
  const uint32_t* word = bottom;
  while (word < top && *word == kStackPaint) {
    ++word;
  }
  return (uint32_t)((uintptr_t)top - (uintptr_t)word);
}

uint32_t StackUsageSize(void) {
  return (uint32_t)((uintptr_t)&__StackTop - (uintptr_t)&__StackBottom);
}

uint32_t StackUsageCore0(void) {
  return StackUsed(&__StackBottom, &__StackTop);
}

uint32_t StackUsageCore1(void) {
  return StackUsed(&__StackOneBottom, &__StackOneTop);
}
