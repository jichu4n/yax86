#include "launcher_menu.h"

#include <stddef.h>
#include <stdio.h>

#include "display.h"
#include "ui_box.h"
#include "ui_page.h"
#include "ui_status.h"

static const UIBoxItem kVMManagerRow = {"Manage VMs", true};
static const UIBoxItem kUSBRow = {"USB mass storage", true};
static const UIBoxItem kSettingsRow = {"Settings", true};

// The rows under the VMs that open a screen of their own.
typedef enum ListRow {
  kListRowManage = 0,
  kListRowUSB,
  kListRowSettings,
  kListRowCount,
} ListRow;

static uint8_t g_chosen;
// Whether what is on the panel is out of date.
static bool g_dirty;
// The row at the top of the list.
static uint8_t g_list_top;
// The VMs to choose from, and the seconds before the highlighted one starts
// by itself.
static const VMList* g_vms;
static uint8_t g_countdown;
// The row that opened the screen showing over the list.
static ListRow g_opened;

const char* LauncherMenuPageName(void) { return "vms"; }

// The list's items: a VM each, then Manage VMs, USB mass storage and Settings,
// with a separator row between the VMs and the rest when there is at least
// one VM.
static uint8_t ItemCount(void) {
  return (uint8_t)(g_vms->count + kListRowCount);
}

static uint8_t RowCount(void) {
  return (uint8_t)(ItemCount() + (g_vms->count > 0 ? 1 : 0));
}

// The item of a row under the VMs.
static uint8_t RowItem(ListRow row) { return (uint8_t)(g_vms->count + row); }

// Keep the chosen item among the list's rows.
static void ScrollToChosen(void) {
  uint8_t row = g_chosen;
  if (g_vms->count > 0 && g_chosen >= g_vms->count) {
    ++row;
  }
  UIPageScrollTo(row, &g_list_top);
}

static void Show(uint8_t chosen) {
  g_chosen = chosen;
  g_dirty = true;
  ScrollToChosen();
}

void LauncherMenuOpen(const VMList* list, uint8_t highlighted) {
  g_vms = list;
  g_list_top = 0;
  Show(highlighted);
}

uint8_t LauncherMenuChosenVM(void) { return g_chosen; }

void LauncherMenuResume(void) { Show(RowItem(g_opened)); }

void LauncherMenuSetCountdown(uint8_t seconds) {
  if (seconds != g_countdown) {
    g_countdown = seconds;
    g_dirty = true;
  }
}

const char* LauncherMenuHint(void) { return "\x18\x19=Move  Enter=Select"; }

static LauncherMenuAction Choose(void) {
  if (g_chosen < g_vms->count) {
    return kLauncherMenuActionVMChosen;
  }
  g_opened = (ListRow)(g_chosen - g_vms->count);
  switch (g_opened) {
    case kListRowManage:
      return kLauncherMenuActionOpenManage;
    case kListRowUSB:
      return kLauncherMenuActionOpenUSB;
    default:
      return kLauncherMenuActionOpenSettings;
  }
}

LauncherMenuAction LauncherMenuHandleKey(SouthbridgeKeyEvent event) {
  // Nothing closes the list but starting something, and Esc goes back no
  // further than it.
  if (event.key == kKeyCodeEsc || event.key == kKeyCodeBackspace) {
    return kLauncherMenuActionNone;
  }
  if (event.state != kSouthbridgeKeyReleased && UIStatusClearProblem()) {
    g_dirty = true;
  }
  // A key held down repeats, which moves through the items as a press does.
  if (event.state == kSouthbridgeKeyReleased) {
    return kLauncherMenuActionNone;
  }
  if (UIPageIsMoveKey(event.key)) {
    if (UIPageMove(event.key, ItemCount(), &g_chosen)) {
      g_dirty = true;
    }
    ScrollToChosen();
    return kLauncherMenuActionNone;
  }
  switch (event.key) {
    case kKeyCodeEnter:
    case kKeyCodeSpace:
      return Choose();
    default:
      return kLauncherMenuActionNone;
  }
}

// The VMs to start: a row each, with a line under them if there are any,
// arrows on the border where there are more above or below, and the
// countdown on the bottom edge.
void LauncherMenuDraw(Y_UNUSED uint64_t now_us) {
  if (!g_dirty) {
    return;
  }
  DisplayMenu* menu = DisplayBeginMenu();
  if (menu == NULL) {
    return;
  }
  UIBoxPlace(menu, kUIPageNarrowColumns, kUIPageListRows + 2);
  UIBoxEdgeRow(&menu->text[0], menu->columns, true, "YAX86");
  for (uint8_t i = 0; i < kUIPageListRows; ++i) {
    DisplayTextRow* row = &menu->text[1 + i];
    const uint8_t index = (uint8_t)(g_list_top + i);
    if (index >= RowCount()) {
      UIBoxBlankRow(row, menu->columns);
    } else if (index < g_vms->count) {
      UIBoxItemRow(
          row, menu->columns, g_vms->vms[index].name, index == g_chosen);
    } else if (g_vms->count > 0 && index == g_vms->count) {
      UIBoxSeparatorRow(row, menu->columns);
    } else {
      const uint8_t item = g_vms->count > 0 ? (uint8_t)(index - 1) : index;
      if (item == RowItem(kListRowManage)) {
        UIBoxMenuItemRow(row, menu->columns, &kVMManagerRow, item == g_chosen);
      } else if (item == RowItem(kListRowUSB)) {
        UIBoxMenuItemRow(row, menu->columns, &kUSBRow, item == g_chosen);
      } else {
        UIBoxMenuItemRow(row, menu->columns, &kSettingsRow, item == g_chosen);
      }
    }
  }
  char countdown[kUIPageNarrowColumns];
  if (g_countdown > 0) {
    snprintf(
        countdown, sizeof(countdown), "starting in %u s",
        (unsigned)g_countdown);
  }
  UIBoxEdgeRow(
      &menu->text[kUIPageListRows + 1], menu->columns, false,
      g_countdown > 0 ? countdown : NULL);
  UIPageDrawScrollArrows(menu, g_list_top, RowCount());
  DisplayEndMenu();
  g_dirty = false;
}
