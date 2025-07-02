#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

void InitBIOS(BIOSState* bios) {
  // Zero out the BIOS state.
  BIOSState empty_bios = {0};
  *bios = empty_bios;
  // TODO: Set BDA values.
}
