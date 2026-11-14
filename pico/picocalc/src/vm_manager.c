#include "vm_manager.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "display.h"
#include "launcher_status_bar.h"
#include "list_box.h"
#include "runtime_log.h"
#include "status_bar.h"

enum {
  kBytesPerKB = 1024,
};

// The copying page's title and what it says is being copied.
static const char* g_copy_title;
static const char* g_copy_what;

void VMManagerNoteChange(
    VMManager* vm_manager, const char* name, bool should_show_vm) {
  vm_manager->did_change = true;
  snprintf(vm_manager->to_select, sizeof(vm_manager->to_select), "%s", name);
  vm_manager->should_show_vm = should_show_vm;
}

void VMManagerShowProblem(
    const char* event, const VMEntry* vm, const char* problem) {
  StatusBarSetProblem(problem);
  SYSTEM_WARN(
      event, Y_LOG_STRING("path", vm->path), Y_LOG_STRING("problem", problem));
}

// Show how far a copy has come, at once: the main loop, which would draw it,
// is waiting for the copy.
static void ShowCopyProgress(uint64_t done, uint64_t total) {
  LauncherStatusBarUpdate("");
  DisplayBox* box = DisplayBeginBox();
  if (box == NULL) {
    return;
  }
  char progress[kListBoxNarrowColumns];
  snprintf(
      progress, sizeof(progress), "%lu of %lu KB",
      (unsigned long)(done / kBytesPerKB),
      (unsigned long)(total / kBytesPerKB));
  const char* const lines[] = {g_copy_what, progress};
  ListBoxDrawItems(box, g_copy_title, lines, 2, NULL, 0, 0, true);
  DisplayEndBox();
}

FSTransfer VMManagerBeginCopy(
    const VMManager* vm_manager, const char* title, const char* what) {
  g_copy_title = title;
  g_copy_what = what;
  return (FSTransfer){
      .buffer = vm_manager->copy_buffer,
      .buffer_size = vm_manager->copy_buffer_size,
      .progress = ShowCopyProgress,
  };
}

void VMManagerEndCopy(const VMManager* vm_manager) {
  memset(vm_manager->copy_buffer, 0, vm_manager->copy_buffer_size);
}
