#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "display_text.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

void InitBIOS(BIOSState* bios, BIOSConfig* config) {
  // Zero out the BIOS state.
  BIOSState empty_bios = {0};
  *bios = empty_bios;

  bios->config = config;

  InitDisplayText(bios);

  // TODO: Set BDA values.
}
