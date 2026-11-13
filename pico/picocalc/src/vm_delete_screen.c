#include "vm_delete_screen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "display.h"
#include "list_box.h"
#include "menu_box.h"
#include "runtime_log.h"

typedef enum DeleteItem {
  kDeleteCancel = 0,
  kDeleteConfirm,
  kDeleteItemCount,
} DeleteItem;

static const MenuBoxItem kDeleteItems[kDeleteItemCount] = {
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
  g_list_box.dirty = true;
  return ScreenOpen(&kVMDeleteScreen);
}

static __noinline ScreenResult Delete(void) {
  const VMEntry vm = g_vm_manager->vms->vms[g_vm];
  char problem[kVMProblemLength + 1];
  const bool deleted = VMListDelete(g_vm_manager->settings, &vm, problem);
  // Gone from the list, it leaves the first VM selected.
  VMManagerNoteChange(g_vm_manager, vm.name, false);
  if (deleted) {
    SYSTEM_INFO("vm_deleted", Y_LOG_STRING("name", vm.name));
  } else {
    VMManagerShowProblem("vm_not_deleted", &vm, problem);
  }
  return ScreenBack();
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxTakeEvent(&g_list_box, event, kDeleteItemCount)) {
    case kListBoxKeySelect:
      if (g_list_box.selected == kDeleteConfirm) {
        return Delete();
      }
      return ScreenBack();
    case kListBoxKeyBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

// Nothing opens over the question.
static ScreenResult OnResume(void) {
  g_list_box.dirty = true;
  return ScreenStays();
}

// Join names into text as "A, B and C", or with fewer shown, "A, B and 2
// more": as many of the first named as fit with the rest counted after.
static void JoinNames(
    char* text, uint8_t size, char (*names)[kFSMaxName + 1], uint8_t named,
    uint8_t count) {
  for (uint8_t shown = named;; --shown) {
    size_t length = 0;
    text[0] = '\0';
    for (uint8_t i = 0; i < shown && length < size; ++i) {
      const char* separator = i == 0 ? "" : (i + 1 == count ? " and " : ", ");
      length += (size_t)snprintf(
          text + length, size - length, "%s%s", separator, names[i]);
    }
    if (shown < count && length < size) {
      length += (size_t)snprintf(
          text + length, size - length, "%s%u more", shown > 0 ? " and " : "",
          (unsigned)(count - shown));
    }
    if (length < size || shown == 0) {
      return;
    }
  }
}

static void OnDraw(Y_UNUSED uint64_t now_us) {
  DisplayMenu* menu = ListBoxBeginDraw(&g_list_box);
  if (menu == NULL) {
    return;
  }
  const uint8_t named = g_contents.file_count < kVMListNamed
                            ? g_contents.file_count
                            : kVMListNamed;
  char question[kListBoxNarrowColumns];
  snprintf(
      question, sizeof(question), "Delete %s?",
      g_vm_manager->vms->vms[g_vm].name);
  char files[kListBoxNarrowColumns - 4];
  JoinNames(
      files, sizeof(files), g_contents.files, named, g_contents.file_count);
  char users[64];
  const char* lines[5] = {question, ""};
  uint8_t line_count = 1;
  if (g_contents.file_count > 0 || g_contents.user_count > 0) {
    ++line_count;
  }
  if (g_contents.file_count > 0) {
    lines[line_count++] = "Deleted with it:";
    lines[line_count++] = files;
  }
  if (g_contents.user_count == 1) {
    snprintf(users, sizeof(users), "%s will lose a disk.", g_contents.users[0]);
    lines[line_count++] = users;
  } else if (g_contents.user_count > 1) {
    snprintf(
        users, sizeof(users), "%s and %u more VMs will lose disks.",
        g_contents.users[0], (unsigned)(g_contents.user_count - 1));
    lines[line_count++] = users;
  }
  ListBoxDrawItems(
      menu, "Delete", lines, line_count, kDeleteItems, kDeleteItemCount,
      g_list_box.selected, true);
  ListBoxEndDraw(&g_list_box);
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return "vm_delete"; }

const Screen kVMDeleteScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .on_resume = OnResume,
    .hint = Hint,
    .page_name = PageName,
};
