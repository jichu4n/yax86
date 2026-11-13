#include "launcher_menu_screen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "display.h"
#include "list_box.h"
#include "menu_box.h"
#include "pico/stdlib.h"
#include "settings_screen.h"
#include "status_bar.h"
#include "usb_storage_screen.h"
#include "vm_manager_screen.h"

enum {
  kMicrosecondsPerSecond = 1000 * 1000,
};

// The menu's rows, from the top: a row for each VM; a separator line, if
// there are VMs; then a row for each MenuRow, each opening a screen of its
// own. The rows that can be selected are its items: the VMs, then the
// MenuRows. An item's index leaves out the separator: with n VMs, item i is
// VM i for i below n, and MenuRow r is item n + r.
typedef enum MenuRow {
  kMenuRowVMManager = 0,
  kMenuRowUSB,
  kMenuRowSettings,
  kMenuRowCount,
} MenuRow;

static const MenuBoxItem kVMManagerRow = {"Manage VMs", true};
static const MenuBoxItem kUSBRow = {"USB mass storage", true};
static const MenuBoxItem kSettingsRow = {"Settings", true};

extern const Screen kLauncherMenuScreen;

// The launcher's, given to LauncherMenuScreenOpen(): the card's VMs and
// settings, the guest RAM copies go through, and where to write why the
// menu closed.
static VMList* g_vms;
static Settings* g_settings;
static uint8_t* g_copy_buffer;
static uint32_t g_copy_buffer_size;
static LauncherMenuExit* g_exit;
// The list box: the item selected, and whether the menu has changed since it
// was last drawn.
static ListBox g_list_box;
// The row shown at the top of the box, past which the menu has scrolled.
static uint8_t g_top_row;
// The countdown: when the selected VM starts by itself, whether it is
// still counting, and the seconds left that the bottom edge shows. A key
// stops it.
static absolute_time_t g_start_at;
static bool g_counting;
static uint8_t g_countdown;
// The row below the VMs whose screen is open over the menu.
static MenuRow g_opened;
// Set by USB mass storage once it has given the card or an image to the
// host, after which the card is read again.
static bool g_exported;

// How many items the menu has, and how many rows, the separator included.
static uint8_t ItemCount(void) {
  return (uint8_t)(g_vms->count + kMenuRowCount);
}

static uint8_t RowCount(void) {
  return (uint8_t)(ItemCount() + (g_vms->count > 0 ? 1 : 0));
}

// The item index of one of the rows below the VMs.
static uint8_t RowItem(MenuRow row) { return (uint8_t)(g_vms->count + row); }

// Scroll the menu, if it has to, so the selected item's row shows.
static void ScrollToSelected(void) {
  uint8_t row = g_list_box.selected;
  if (g_vms->count > 0 && g_list_box.selected >= g_vms->count) {
    ++row;
  }
  ListBoxScrollTo(row, &g_top_row);
}

// Select item, scrolling the menu to it if it must.
static void SetSelected(uint8_t item) {
  g_list_box.selected = item;
  g_list_box.dirty = true;
  ScrollToSelected();
}

ScreenResult LauncherMenuScreenOpen(
    VMList* vms, Settings* settings, uint8_t* copy_buffer,
    uint32_t copy_buffer_size, uint8_t selected, uint32_t countdown_ms,
    LauncherMenuExit* exit) {
  g_vms = vms;
  g_settings = settings;
  g_copy_buffer = copy_buffer;
  g_copy_buffer_size = copy_buffer_size;
  g_exit = exit;
  g_top_row = 0;
  SetSelected(selected);
  g_start_at = make_timeout_time_ms(countdown_ms);
  g_counting = countdown_ms > 0;
  return ScreenOpen(&kLauncherMenuScreen);
}

// Show the seconds left before the selected VM starts by itself on the
// bottom edge, or with 0, nothing there.
static void SetCountdown(uint8_t seconds) {
  if (seconds != g_countdown) {
    g_countdown = seconds;
    g_list_box.dirty = true;
  }
}

// Close the menu to start the selected VM.
static ScreenResult StartSelected(void) {
  *g_exit = (LauncherMenuExit){kLauncherMenuExitStartVM, g_list_box.selected};
  return ScreenClose();
}

// Enter or Space: start the selected VM, or open the selected row's screen
// over the menu.
static ScreenResult OnSelect(void) {
  if (g_list_box.selected < g_vms->count) {
    return StartSelected();
  }
  g_opened = (MenuRow)(g_list_box.selected - g_vms->count);
  switch (g_opened) {
    case kMenuRowVMManager:
      return VMManagerScreenOpen(
          g_vms, g_settings, g_copy_buffer, g_copy_buffer_size);
    case kMenuRowUSB:
      return USBStorageScreenOpen(&g_exported);
    default:
      return SettingsScreenOpen(g_settings);
  }
}

// Any key stops the countdown. Up and Down move the selection, and Enter or
// Space acts on it.
static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  g_counting = false;
  SetCountdown(0);
  // Esc and Backspace do nothing: the menu is the first screen, with nothing
  // under it to go back to.
  if (event.key == kKeyCodeEsc || event.key == kKeyCodeBackspace) {
    return ScreenStays();
  }
  if (event.state != kSouthbridgeKeyReleased && StatusBarClearProblem()) {
    g_list_box.dirty = true;
  }
  // A key held down repeats, which moves through the items as a press does.
  if (event.state == kSouthbridgeKeyReleased) {
    return ScreenStays();
  }
  switch (ListBoxTakeKey(&g_list_box, event.key, ItemCount())) {
    case kListBoxKeyMoved:
      ScrollToSelected();
      return ScreenStays();
    case kListBoxKeySelect:
      return OnSelect();
    default:
      return ScreenStays();
  }
}

// Run the countdown, showing the seconds left, and start the selected VM
// once they run out.
static ScreenResult OnTick(void) {
  if (!g_counting) {
    return ScreenStays();
  }
  const int64_t left_us =
      absolute_time_diff_us(get_absolute_time(), g_start_at);
  if (left_us <= 0) {
    g_counting = false;
    SetCountdown(0);
    return StartSelected();
  }
  SetCountdown((uint8_t)((left_us + kMicrosecondsPerSecond - 1) /
                         kMicrosecondsPerSecond));
  return ScreenStays();
}

// A screen opened over the menu has closed. After USB mass storage gave the
// card to the host, close the menu, for the launcher to read the card again.
// Otherwise select the row that opened the screen again: the VM manager may
// have changed how many VMs there are, which moves it.
static ScreenResult OnResume(void) {
  if (g_exported) {
    g_exported = false;
    *g_exit = (LauncherMenuExit){kLauncherMenuExitReadCard, 0};
    return ScreenClose();
  }
  SetSelected(RowItem(g_opened));
  return ScreenStays();
}

// Draw the rows that fit in the box from g_top_row, with arrows on its border
// where there are more rows above or below, and the countdown on its bottom
// edge.
static bool OnDraw(DisplayMenu* menu, Y_UNUSED uint64_t now_us) {
  if (!ListBoxTakeDirty(&g_list_box)) {
    return false;
  }
  MenuBoxPlace(menu, kListBoxNarrowColumns, kListBoxRows + 2);
  MenuBoxEdgeRow(&menu->text[0], menu->columns, true, "YAX86");
  for (uint8_t i = 0; i < kListBoxRows; ++i) {
    DisplayTextRow* row = &menu->text[1 + i];
    const uint8_t index = (uint8_t)(g_top_row + i);
    if (index >= RowCount()) {
      MenuBoxBlankRow(row, menu->columns);
    } else if (index < g_vms->count) {
      MenuBoxItemTextRow(
          row, menu->columns, g_vms->vms[index].name,
          index == g_list_box.selected);
    } else if (g_vms->count > 0 && index == g_vms->count) {
      MenuBoxSeparatorRow(row, menu->columns);
    } else {
      const uint8_t item = g_vms->count > 0 ? (uint8_t)(index - 1) : index;
      const bool selected = item == g_list_box.selected;
      if (item == RowItem(kMenuRowVMManager)) {
        MenuBoxItemRow(row, menu->columns, &kVMManagerRow, selected);
      } else if (item == RowItem(kMenuRowUSB)) {
        MenuBoxItemRow(row, menu->columns, &kUSBRow, selected);
      } else {
        MenuBoxItemRow(row, menu->columns, &kSettingsRow, selected);
      }
    }
  }
  char countdown[kListBoxNarrowColumns];
  if (g_countdown > 0) {
    snprintf(
        countdown, sizeof(countdown), "starting in %u s",
        (unsigned)g_countdown);
  }
  MenuBoxEdgeRow(
      &menu->text[kListBoxRows + 1], menu->columns, false,
      g_countdown > 0 ? countdown : NULL);
  ListBoxDrawScrollArrows(menu, g_top_row, RowCount());
  return true;
}

static const char* Hint(void) { return "\x18\x19=Move  Enter=Select"; }

static const char* PageName(void) { return "vms"; }

const Screen kLauncherMenuScreen = {
    .on_key = OnKey,
    .on_tick = OnTick,
    .on_draw = OnDraw,
    .on_resume = OnResume,
    .hint = Hint,
    .page_name = PageName,
};
