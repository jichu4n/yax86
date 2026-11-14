#include "vm_name_screen.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>

#include "box.h"
#include "display.h"
#include "list_box.h"
#include "pico/stdlib.h"
#include "runtime_log.h"
#include "status_bar.h"

enum {
  kMicrosecondsPerMillisecond = 1000,
};

extern const Screen kVMNameScreen;

static VMManager* g_vm_manager;
// The list box, for whether it is out of date. It has no items to select.
static ListBox g_list_box;
// The VM, an index in the list; the name being typed; and whether it is for
// a clone rather than a rename.
static uint8_t g_vm;
static char g_name[kFSMaxFolderName + 1];
static uint8_t g_name_length;
static bool g_cloning;

ScreenResult VMNameScreenOpen(
    VMManager* vm_manager, uint8_t index, bool cloning) {
  g_vm_manager = vm_manager;
  g_vm = index;
  g_cloning = cloning;
  const char* name = vm_manager->vms->vms[index].name;
  g_name_length = 0;
  while (name[g_name_length] != '\0' && g_name_length < kFSMaxFolderName) {
    g_name[g_name_length] = name[g_name_length];
    ++g_name_length;
  }
  // A clone's name has _2 at the end, in the name's 8 characters.
  if (cloning) {
    if (g_name_length > kFSMaxFolderName - 2) {
      g_name_length = kFSMaxFolderName - 2;
    }
    g_name[g_name_length++] = '_';
    g_name[g_name_length++] = '2';
  }
  g_name[g_name_length] = '\0';
  g_list_box.dirty = true;
  return ScreenOpen(&kVMNameScreen);
}

static __noinline ScreenResult Rename(void) {
  const VMEntry vm = g_vm_manager->vms->vms[g_vm];
  VMEntry renamed;
  char problem[kVMProblemLength + 1];
  const VMListResult result = VMListRename(
      g_vm_manager->vms, g_vm_manager->settings, &vm, g_name, &renamed,
      problem);
  if (result == kVMListRefused) {
    StatusBarSetProblem(problem);
    return ScreenStays();
  }
  VMManagerNoteChange(
      g_vm_manager, result == kVMListDone ? renamed.name : vm.name, false);
  if (result == kVMListDone) {
    SYSTEM_INFO(
        "vm_renamed", Y_LOG_STRING("from", vm.name),
        Y_LOG_STRING("to", renamed.name));
  } else {
    VMManagerShowProblem("vm_not_renamed", &vm, problem);
  }
  return ScreenBack();
}

static __noinline ScreenResult Clone(void) {
  const VMEntry vm = g_vm_manager->vms->vms[g_vm];
  VMEntry copy;
  char problem[kVMProblemLength + 1];
  char what[kListBoxNarrowColumns];
  snprintf(what, sizeof(what), "Copying %s to %s...", vm.name, g_name);
  const uint64_t start_us = time_us_64();
  FSTransfer transfer = VMManagerBeginCopy(g_vm_manager, "Clone", what);
  const VMListResult result =
      VMListClone(&vm, g_name, &transfer, &copy, problem);
  VMManagerEndCopy(g_vm_manager);
  if (result == kVMListRefused) {
    StatusBarSetProblem(problem);
    return ScreenStays();
  }
  VMManagerNoteChange(
      g_vm_manager, result == kVMListDone ? copy.name : vm.name, false);
  if (result == kVMListDone) {
    SYSTEM_INFO(
        "vm_cloned", Y_LOG_STRING("from", vm.name),
        Y_LOG_STRING("to", copy.name),
        Y_LOG_UINT(
            "ms", (time_us_64() - start_us) / kMicrosecondsPerMillisecond));
  } else {
    VMManagerShowProblem("vm_not_cloned", &vm, problem);
  }
  return ScreenBack();
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  if (!ListBoxIsKeyPress(event)) {
    return ScreenStays();
  }
  const uint8_t key = event.key;
  switch (key) {
    case kKeyCodeEnter:
      return g_cloning ? Clone() : Rename();
    case kKeyCodeEsc:
      return ScreenBack();
    case kKeyCodeBackspace:
      if (g_name_length > 0) {
        g_name[--g_name_length] = '\0';
        g_list_box.dirty = true;
      }
      return ScreenStays();
    default:
      break;
  }
  if (key < 0x80 && FSIsFolderNameCharacter((char)key) &&
      g_name_length < kFSMaxFolderName) {
    g_name[g_name_length++] = (char)toupper(key);
    g_name[g_name_length] = '\0';
    g_list_box.dirty = true;
  }
  return ScreenStays();
}

// Nothing opens over the name.
static ScreenResult OnResume(void) {
  g_list_box.dirty = true;
  return ScreenStays();
}

// The name being typed, in a field marked as the item selected is, with a
// cursor after it while there is room.
static void OnDraw(Y_UNUSED uint64_t now_us) {
  DisplayBox* box = ListBoxBeginDraw(&g_list_box);
  if (box == NULL) {
    return;
  }
  char title[kListBoxNarrowColumns];
  snprintf(
      title, sizeof(title), "%s %s", g_cloning ? "Clone" : "Rename",
      g_vm_manager->vms->vms[g_vm].name);
  const char* const lines[] = {
      g_cloning ? "Name of the copy:" : "New name:",
      "",
      "",
      "Up to 8 letters, digits, - and _.",
  };
  ListBoxDrawItems(
      box, title, lines, sizeof(lines) / sizeof(lines[0]), NULL, 0, 0, true);
  // The field, on the blank line: the box's edge, a blank row, then the
  // first line.
  DisplayTextRow* row = &box->text[3];
  char field[kFSMaxFolderName + 1];
  for (uint8_t i = 0; i < kFSMaxFolderName; ++i) {
    field[i] = i < g_name_length ? g_name[i] : ' ';
  }
  if (g_name_length < kFSMaxFolderName) {
    field[g_name_length] = '_';
  }
  field[kFSMaxFolderName] = '\0';
  BoxPutText(row, box->columns, 3, field, kBoxAttrSelected);
  ListBoxEndDraw(&g_list_box);
}

static const char* Hint(void) {
  return g_cloning ? "Enter=Clone  Backspace=Erase  Esc=Cancel"
                   : "Enter=Rename  Backspace=Erase  Esc=Cancel";
}

static const char* PageName(void) { return "vm_name"; }

const Screen kVMNameScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .on_resume = OnResume,
    .hint = Hint,
    .page_name = PageName,
};
