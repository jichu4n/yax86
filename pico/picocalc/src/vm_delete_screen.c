#include "vm_delete_screen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "box.h"
#include "display.h"
#include "list_box.h"
#include "runtime_log.h"

typedef enum DeleteItem {
  kDeleteCancel = 0,
  kDeleteConfirm,
  kNumDeleteItems,
} DeleteItem;

static const BoxItem kDeleteItems[kNumDeleteItems] = {
    [kDeleteCancel] = {"Cancel", false},
    [kDeleteConfirm] = {"Delete", false},
};

extern const Screen kVMDeleteScreen;

static VMManager* g_vm_manager;
// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;
// The VM, an index in the list, and what deleting it would take.
static uint8_t g_vm;
static VMListContents g_contents;

ScreenResult VMDeleteScreenOpen(VMManager* vm_manager, uint8_t index) {
  g_vm_manager = vm_manager;
  g_vm = index;
  VMListDescribe(vm_manager->vms, &vm_manager->vms->vms[index], &g_contents);
  g_list_box.selected = kDeleteCancel;
  return ScreenOpen(&kVMDeleteScreen);
}

static __noinline ScreenResult Delete(void) {
  const VMEntry vm = g_vm_manager->vms->vms[g_vm];
  char problem[kVMProblemLength + 1];
  const bool was_deleted = VMListDelete(g_vm_manager->settings, &vm, problem);
  // Gone from the list, it leaves the first VM selected.
  VMManagerNoteChange(g_vm_manager, vm.name, false);
  if (was_deleted) {
    SYSTEM_INFO("vm_deleted", Y_LOG_STRING("name", vm.name));
  } else {
    VMManagerShowProblem("vm_not_deleted", &vm, problem);
  }
  return ScreenBack();
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxHandleKeyEvent(&g_list_box, event, kNumDeleteItems)) {
    case kListBoxActionSelect:
      if (g_list_box.selected == kDeleteConfirm) {
        return Delete();
      }
      return ScreenBack();
    case kListBoxActionBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

// Join names into text as "A, B and C", or with fewer shown, "A, B and 2
// more": as many of the first named as fit with the rest counted after.
static void JoinNames(
    char* text, uint8_t size, char (*names)[kFSMaxName + 1], uint8_t named,
    uint8_t num_named) {
  for (uint8_t shown = named;; --shown) {
    size_t length = 0;
    text[0] = '\0';
    for (uint8_t i = 0; i < shown && length < size; ++i) {
      const char* separator =
          i == 0 ? "" : (i + 1 == num_named ? " and " : ", ");
      length += (size_t)snprintf(
          text + length, size - length, "%s%s", separator, names[i]);
    }
    if (shown < num_named && length < size) {
      length += (size_t)snprintf(
          text + length, size - length, "%s%u more", shown > 0 ? " and " : "",
          (unsigned)(num_named - shown));
    }
    if (length < size || shown == 0) {
      return;
    }
  }
}

static void OnDraw(DisplayBox* box) {
  const uint8_t named = g_contents.num_files < kVMListNumNamed
                            ? g_contents.num_files
                            : kVMListNumNamed;
  char question[kListBoxNarrowColumns];
  snprintf(
      question, sizeof(question), "Delete %s?",
      g_vm_manager->vms->vms[g_vm].name);
  char files[kListBoxNarrowColumns - 4];
  JoinNames(
      files, sizeof(files), g_contents.files, named, g_contents.num_files);
  char users[64];
  const char* lines[5] = {question, ""};
  uint8_t num_lines = 1;
  if (g_contents.num_files > 0 || g_contents.num_users > 0) {
    ++num_lines;
  }
  if (g_contents.num_files > 0) {
    lines[num_lines++] = "Deleted with it:";
    lines[num_lines++] = files;
  }
  if (g_contents.num_users == 1) {
    snprintf(users, sizeof(users), "%s will lose a disk.", g_contents.users[0]);
    lines[num_lines++] = users;
  } else if (g_contents.num_users > 1) {
    snprintf(
        users, sizeof(users), "%s and %u more VMs will lose disks.",
        g_contents.users[0], (unsigned)(g_contents.num_users - 1));
    lines[num_lines++] = users;
  }
  ListBoxDrawItems(
      box, "Delete", lines, num_lines, kDeleteItems, kNumDeleteItems,
      g_list_box.selected, true);
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return "vm_delete"; }

const Screen kVMDeleteScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .hint = Hint,
    .page_name = PageName,
    .is_dirty = &g_list_box.is_dirty,
};
