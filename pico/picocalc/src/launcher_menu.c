#include "launcher_menu.h"

#include <stddef.h>
#include <stdio.h>

#include "display.h"
#include "launcher.h"
#include "ui_box.h"
#include "ui_page.h"

static const UIBoxItem kManageRow = {"Manage VMs", true};
static const UIBoxItem kUSBRow = {"USB disk mode", true};
static const UIBoxItem kSettingsRow = {"Settings", true};

// Whether the list shows: from LauncherMenuOpen() until LauncherMenuClose().
static bool g_open;
static uint8_t g_chosen;
// Whether what is on the panel is out of date.
static bool g_dirty;
// The row at the top of the list.
static uint8_t g_list_top;
// The VMs to choose from at power on, the seconds before the
// highlighted one starts by itself, and why the last chosen did not.
static const VMList* g_vms;
static uint8_t g_countdown;

bool LauncherMenuIsOpen(void) { return g_open; }

const char* LauncherMenuPageName(void) { return "vms"; }

static void ScrollToChosen(void);

void LauncherMenuOpen(const VMList* list, uint8_t highlighted) {
  g_vms = list;
  g_open = true;
  g_list_top = 0;
  g_chosen = highlighted;
  g_dirty = true;
  ScrollToChosen();
}

uint8_t LauncherMenuChosenVM(void) { return g_chosen; }

void LauncherMenuClose(void) { g_open = false; }

void LauncherMenuSetCountdown(uint8_t seconds) {
  if (seconds != g_countdown) {
    g_countdown = seconds;
    g_dirty = true;
  }
}

const char* LauncherMenuHint(void) { return "\x18\x19=Move  Enter=Select"; }

// Keep the chosen item among the list's rows.
static void ScrollToChosen(void) {
  uint8_t row = g_chosen;
  if (g_vms->count > 0 && g_chosen >= g_vms->count) {
    ++row;
  }
  UIPageScrollTo(row, &g_list_top);
}

// The list at power on's items: a VM each, then Manage VMs, USB disk mode and
// Settings, with a separator row between the VMs and the rest when there is at
// least one VM.
static uint8_t VMsItemCount(void) { return (uint8_t)(g_vms->count + 3); }

static uint8_t VMsRowCount(void) {
  return (uint8_t)(g_vms->count + (g_vms->count > 0 ? 4 : 3));
}

static uint8_t ManageItemIndex(void) { return g_vms->count; }

static uint8_t USBItemIndex(void) { return (uint8_t)(g_vms->count + 1); }

static uint8_t SettingsItemIndex(void) { return (uint8_t)(g_vms->count + 2); }

// Show the list again with an item chosen, when what was opened from it is
// left.
static void ShowItem(uint8_t item) {
  g_chosen = item;
  g_dirty = true;
  ScrollToChosen();
}

void LauncherMenuShowManageRow(void) { ShowItem(ManageItemIndex()); }

void LauncherMenuShowUSBRow(void) { ShowItem(USBItemIndex()); }

void LauncherMenuShowSettingsRow(void) { ShowItem(SettingsItemIndex()); }

static LauncherMenuAction Choose(void) {
  if (g_chosen == SettingsItemIndex()) {
    return kLauncherMenuActionOpenSettings;
  }
  if (g_chosen == USBItemIndex()) {
    return kLauncherMenuActionOpenUSB;
  }
  if (g_chosen == ManageItemIndex()) {
    return kLauncherMenuActionOpenManage;
  }
  return kLauncherMenuActionVMChosen;
}

LauncherMenuAction LauncherMenuHandleKey(SouthbridgeKeyEvent event) {
  // Nothing closes the list but starting something, and Esc goes back no
  // further than it.
  if (event.key == kKeyCodeEsc || event.key == kKeyCodeBackspace) {
    return kLauncherMenuActionNone;
  }
  if (event.state != kSouthbridgeKeyReleased && LauncherClearProblem()) {
    g_dirty = true;
  }
  // A key held down repeats, which moves through the items as a press does.
  if (event.state == kSouthbridgeKeyReleased) {
    return kLauncherMenuActionNone;
  }
  if (UIPageIsMoveKey(event.key)) {
    if (UIPageMove(event.key, VMsItemCount(), &g_chosen)) {
      g_dirty = true;
    }
    ScrollToChosen();
    return kLauncherMenuActionNone;
  }
  if (event.key == kKeyCodeEnter || event.key == kKeyCodeSpace) {
    return Choose();
  }
  return kLauncherMenuActionNone;
}

// ============================================================================
// Drawing
// ============================================================================

// The VMs to start: a row each, with a line under them if there are any,
// arrows on the border where there are more above or below, and the
// countdown on the bottom edge.
static void DrawVMsPage(DisplayMenu* menu) {
  UIBoxPlace(menu, kUIPageNarrowColumns, kUIPageListRows + 2);
  UIBoxEdgeRow(&menu->text[0], menu->columns, true, "YAX86");
  for (uint8_t i = 0; i < kUIPageListRows; ++i) {
    DisplayTextRow* row = &menu->text[1 + i];
    const uint8_t index = (uint8_t)(g_list_top + i);
    if (index >= VMsRowCount()) {
      UIBoxBlankRow(row, menu->columns);
    } else if (index < g_vms->count) {
      UIBoxItemRow(
          row, menu->columns, g_vms->vms[index].name, index == g_chosen);
    } else if (g_vms->count > 0 && index == g_vms->count) {
      UIBoxSeparatorRow(row, menu->columns);
    } else {
      const uint8_t item = g_vms->count > 0 ? (uint8_t)(index - 1) : index;
      if (item == ManageItemIndex()) {
        UIBoxMenuItemRow(row, menu->columns, &kManageRow, item == g_chosen);
      } else if (item == USBItemIndex()) {
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
  UIPageDrawScrollArrows(menu, g_list_top, VMsRowCount());
}

void LauncherMenuDraw(void) {
  if (!g_open || !g_dirty) {
    return;
  }
  DisplayMenu* menu = DisplayBeginMenu();
  if (menu == NULL) {
    return;
  }
  DrawVMsPage(menu);
  // The list is always the same size, so there is never part of another box
  // left showing to draw over.
  DisplayEndMenu();
  g_dirty = false;
}
