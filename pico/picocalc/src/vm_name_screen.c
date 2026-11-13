#include "vm_name_screen.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>

#include "display.h"
#include "pico/stdlib.h"
#include "runtime_log.h"
#include "status_bar.h"
#include "ui_box.h"
#include "ui_page.h"

enum {
  kMicrosecondsPerMillisecond = 1000,
};

extern const Screen kVMNameScreen;

static ManageState* g_manage;
// Whether what is on the panel is out of date.
static bool g_dirty;
// The VM, an index in the list; the name being typed; and whether it is for
// a clone rather than a rename.
static uint8_t g_vm;
static char g_name[kFSMaxFolderName + 1];
static uint8_t g_name_length;
static bool g_cloning;

ScreenResult VMNameScreenOpen(
    ManageState* manage, uint8_t index, bool cloning) {
  g_manage = manage;
  g_vm = index;
  g_cloning = cloning;
  const char* name = manage->vms->vms[index].name;
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
  g_dirty = true;
  return ScreenOpen(&kVMNameScreen);
}

static __noinline ScreenResult Rename(void) {
  const VMEntry vm = g_manage->vms->vms[g_vm];
  VMEntry renamed;
  char problem[kVMProblemLength + 1];
  const VMListResult result = VMListRename(
      g_manage->vms, g_manage->settings, &vm, g_name, &renamed, problem);
  if (result == kVMListRefused) {
    StatusBarSetProblem(problem);
    return ScreenStays();
  }
  ManageNoteChange(
      g_manage, result == kVMListDone ? renamed.name : vm.name, false);
  if (result == kVMListDone) {
    SYSTEM_INFO(
        "vm_renamed", Y_LOG_STRING("from", vm.name),
        Y_LOG_STRING("to", renamed.name));
  } else {
    ManageShowProblem("vm_not_renamed", &vm, problem);
  }
  return ScreenBack();
}

static __noinline ScreenResult Clone(void) {
  const VMEntry vm = g_manage->vms->vms[g_vm];
  VMEntry copy;
  char problem[kVMProblemLength + 1];
  char what[kUIPageNarrowColumns];
  snprintf(what, sizeof(what), "Copying %s to %s...", vm.name, g_name);
  const uint64_t start_us = time_us_64();
  FSTransfer transfer = ManageBeginCopy(g_manage, "Clone", what);
  const VMListResult result =
      VMListClone(&vm, g_name, &transfer, &copy, problem);
  ManageEndCopy(g_manage);
  if (result == kVMListRefused) {
    StatusBarSetProblem(problem);
    return ScreenStays();
  }
  ManageNoteChange(
      g_manage, result == kVMListDone ? copy.name : vm.name, false);
  if (result == kVMListDone) {
    SYSTEM_INFO(
        "vm_cloned", Y_LOG_STRING("from", vm.name),
        Y_LOG_STRING("to", copy.name),
        Y_LOG_UINT(
            "ms", (time_us_64() - start_us) / kMicrosecondsPerMillisecond));
  } else {
    ManageShowProblem("vm_not_cloned", &vm, problem);
  }
  return ScreenBack();
}

static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  // A key held down repeats, as a press does.
  if (event.state == kSouthbridgeKeyReleased) {
    return ScreenStays();
  }
  StatusBarClearProblem();
  const uint8_t key = event.key;
  switch (key) {
    case kKeyCodeEnter:
      return g_cloning ? Clone() : Rename();
    case kKeyCodeEsc:
      return ScreenBack();
    case kKeyCodeBackspace:
      if (g_name_length > 0) {
        g_name[--g_name_length] = '\0';
        g_dirty = true;
      }
      return ScreenStays();
    default:
      break;
  }
  if (key < 0x80 && FSIsFolderNameCharacter((char)key) &&
      g_name_length < kFSMaxFolderName) {
    g_name[g_name_length++] = (char)toupper(key);
    g_name[g_name_length] = '\0';
    g_dirty = true;
  }
  return ScreenStays();
}

// Nothing opens over the name.
static ScreenResult Resume(void) {
  g_dirty = true;
  return ScreenStays();
}

// The name being typed, in a field marked as the item chosen is, with a
// cursor after it while there is room.
static void Draw(Y_UNUSED uint64_t now_us) {
  if (!g_dirty) {
    return;
  }
  DisplayMenu* menu = DisplayBeginMenu();
  if (menu == NULL) {
    return;
  }
  char title[kUIPageNarrowColumns];
  snprintf(
      title, sizeof(title), "%s %s", g_cloning ? "Clone" : "Rename",
      g_manage->vms->vms[g_vm].name);
  const char* const lines[] = {
      g_cloning ? "Name of the copy:" : "New name:",
      "",
      "",
      "Up to 8 letters, digits, - and _.",
  };
  UIPageDrawItems(
      menu, title, lines, sizeof(lines) / sizeof(lines[0]), NULL, 0, 0, true);
  // The field, on the blank line: the box's edge, a blank row, then the
  // first line.
  DisplayTextRow* row = &menu->text[3];
  char field[kFSMaxFolderName + 1];
  for (uint8_t i = 0; i < kFSMaxFolderName; ++i) {
    field[i] = i < g_name_length ? g_name[i] : ' ';
  }
  if (g_name_length < kFSMaxFolderName) {
    field[g_name_length] = '_';
  }
  field[kFSMaxFolderName] = '\0';
  UIBoxPutText(row, menu->columns, 3, field, kUIBoxAttrChosen);
  DisplayEndMenu();
  g_dirty = false;
}

static const char* Hint(void) {
  return g_cloning ? "Enter=Clone  Backspace=Erase  Esc=Cancel"
                   : "Enter=Rename  Backspace=Erase  Esc=Cancel";
}

static const char* PageName(void) { return "vm_name"; }

const Screen kVMNameScreen = {
    .handle_key = HandleKey,
    .draw = Draw,
    .resume = Resume,
    .hint = Hint,
    .page_name = PageName,
};
