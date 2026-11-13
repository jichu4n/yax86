#include "vm_manager.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "display.h"
#include "launcher_status_bar.h"
#include "runtime_log.h"
#include "status_bar.h"
#include "ui_page.h"

enum {
  kBytesPerKB = 1024,
};

// The copying page's title and what it says is being copied.
static const char* g_copy_title;
static const char* g_copy_what;

void VMManagerNoteChange(
    VMManager* vm_manager, const char* name, bool show_vm) {
  vm_manager->changed = true;
  snprintf(vm_manager->chosen, sizeof(vm_manager->chosen), "%s", name);
  vm_manager->show_vm = show_vm;
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
  DisplayMenu* menu = DisplayBeginMenu();
  if (menu == NULL) {
    return;
  }
  char progress[kUIPageNarrowColumns];
  snprintf(
      progress, sizeof(progress), "%lu of %lu KB",
      (unsigned long)(done / kBytesPerKB),
      (unsigned long)(total / kBytesPerKB));
  const char* const lines[] = {g_copy_what, progress};
  UIPageDrawItems(menu, g_copy_title, lines, 2, NULL, 0, 0, true);
  DisplayEndMenu();
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
