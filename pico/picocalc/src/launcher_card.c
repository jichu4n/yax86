#include "launcher_card.h"

#include <stdio.h>
#include <strings.h>

#include "display.h"
#include "pico/stdlib.h"
#include "runtime_log.h"
#include "status_bar.h"
#include "text.h"
#include "ui_status.h"

// Kept out of line, so that its message is on core 0's stack only while it
// runs rather than in the frames of the callers it would be inlined into.
__noinline bool LauncherCardReadSettings(
    const VMList* vms, Settings* settings, uint8_t* highlighted) {
  char problem[kVMProblemLength + 1];
  bool read = SettingsRead(settings, problem);
  DisplaySetFont(settings->font);
  *highlighted = 0;
  if (read && settings->boot_default[0] != '\0') {
    // Past the list's end, the first is highlighted instead.
    const uint8_t index = VMListFind(vms, settings->boot_default);
    if (index < kVMListMax) {
      *highlighted = index;
    }
  }
  if (!read) {
    char message[kTextColumns + 1];
    snprintf(message, sizeof(message), "YAX86.CFG: %s", problem);
    UIStatusSetProblem(message);
  }
  return read;
}

// Make the VM named name the one to start at the next power on, if it is not
// already. Writing the card only when it changes keeps a board that starts
// its default every time from writing at all.
static void SaveBootDefault(Settings* settings, const char* name) {
  if (strcasecmp(settings->boot_default, name) == 0) {
    return;
  }
  char problem[kVMProblemLength + 1];
  if (SettingsSaveBootDefault(settings, name, problem)) {
    SYSTEM_INFO("boot_default_saved", Y_LOG_STRING("name", name));
    return;
  }
  char message[kTextColumns + 1];
  snprintf(
      message, sizeof(message), "Could not save boot_default: %.50s", problem);
  StatusBarPostMessage(message);
  SYSTEM_WARN("boot_default_not_saved", Y_LOG_STRING("problem", problem));
}

__noinline bool LauncherCardTakeVM(
    const VMList* vms, Settings* settings, uint8_t index, VMConfig* chosen) {
  const VMEntry* vm = &vms->vms[index];
  char problem[kVMProblemLength + 1];
  if (VMConfigRead(vm, chosen, problem)) {
    SYSTEM_INFO("vm_chosen", Y_LOG_STRING("path", vm->path));
    SaveBootDefault(settings, vm->name);
    return true;
  }
  char message[kTextColumns + 1];
  snprintf(message, sizeof(message), "%s: %s", vm->name, problem);
  UIStatusSetProblem(message);
  SYSTEM_WARN(
      "vm_config_problem", Y_LOG_STRING("path", vm->path),
      Y_LOG_STRING("problem", problem));
  return false;
}
