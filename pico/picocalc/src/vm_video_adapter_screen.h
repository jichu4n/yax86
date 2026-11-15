// Video adapter, from a VM's configuration: CGA or MDA.
#ifndef YAX86_PICOCALC_VM_VIDEO_ADAPTER_SCREEN_H
#define YAX86_PICOCALC_VM_VIDEO_ADAPTER_SCREEN_H

#include "core/video.h"
#include "screen.h"

// Show the video adapters, with *chosen selected. Choosing one writes it to
// *chosen before the screen goes back. chosen must outlive the screen.
ScreenResult VMVideoAdapterScreenOpen(YVideoAdapter* chosen);

#endif  // YAX86_PICOCALC_VM_VIDEO_ADAPTER_SCREEN_H
