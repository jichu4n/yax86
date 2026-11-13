#include "vm_delete_screen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "display.h"
#include "runtime_log.h"
#include "ui_box.h"
#include "ui_page.h"

typedef enum DeleteItem {
  kDeleteCancel = 0,
  kDeleteConfirm,
  kDeleteItemCount,
} DeleteItem;

static const UIBoxItem kDeleteItems[kDeleteItemCount] = {
    [kDeleteCancel] = {"Cancel", false},
    [kDeleteConfirm] = {"Delete", false},
};

extern const Screen kVMDeleteScreen;

static ManageState* g_manage;
// The item chosen, and whether the page is out of date.
static UIPage g_page;
// The VM, an index in the list, and what deleting it would take.
static uint8_t g_vm;
static VMListContents g_contents;

ScreenResult VMDeleteScreenOpen(ManageState* manage, uint8_t index) {
  g_manage = manage;
  g_vm = index;
  VMListDescribe(manage->vms, &manage->vms->vms[index], &g_contents);
  g_page.chosen = kDeleteCancel;
  g_page.dirty = true;
  return ScreenOpen(&kVMDeleteScreen);
}

static __noinline ScreenResult Delete(void) {
  const VMEntry vm = g_manage->vms->vms[g_vm];
  char problem[kVMProblemLength + 1];
  const bool deleted = VMListDelete(g_manage->settings, &vm, problem);
  // Gone from the list, it leaves the first VM chosen.
  ManageNoteChange(g_manage, vm.name, false);
  if (deleted) {
    SYSTEM_INFO("vm_deleted", Y_LOG_STRING("name", vm.name));
  } else {
    ManageShowProblem("vm_not_deleted", &vm, problem);
  }
  return ScreenBack();
}

static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  switch (UIPageTakeEvent(&g_page, event, kDeleteItemCount)) {
    case kUIPageKeyChoose:
      if (g_page.chosen == kDeleteConfirm) {
        return Delete();
      }
      return ScreenBack();
    case kUIPageKeyBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

// Nothing opens over the question.
static ScreenResult Resume(void) {
  g_page.dirty = true;
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

static void Draw(Y_UNUSED uint64_t now_us) {
  DisplayMenu* menu = UIPageBeginDraw(&g_page);
  if (menu == NULL) {
    return;
  }
  const uint8_t named = g_contents.file_count < kVMListNamed
                            ? g_contents.file_count
                            : kVMListNamed;
  char question[kUIPageNarrowColumns];
  snprintf(
      question, sizeof(question), "Delete %s?", g_manage->vms->vms[g_vm].name);
  char files[kUIPageNarrowColumns - 4];
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
  UIPageDrawItems(
      menu, "Delete", lines, line_count, kDeleteItems, kDeleteItemCount,
      g_page.chosen, true);
  UIPageEndDraw(&g_page);
}

static const char* Hint(void) { return kUIPageHint; }

static const char* PageName(void) { return "vm_delete"; }

const Screen kVMDeleteScreen = {
    .handle_key = HandleKey,
    .draw = Draw,
    .resume = Resume,
    .hint = Hint,
    .page_name = PageName,
};
