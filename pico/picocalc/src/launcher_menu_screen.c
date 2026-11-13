#include "launcher_menu_screen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "display.h"
#include "pico/stdlib.h"
#include "settings_screen.h"
#include "ui_box.h"
#include "ui_page.h"
#include "ui_status.h"
#include "usb_storage_screen.h"
#include "vm_manager_screen.h"

enum {
  kMicrosecondsPerSecond = 1000 * 1000,
};

// The menu's rows, from the top: a row for each VM; a separator line, if
// there are VMs; then a row for each MenuRow, each opening a screen of its
// own. The rows that can be highlighted are its items: the VMs, then the
// MenuRows. An item's index leaves out the separator: with n VMs, item i is
// VM i for i below n, and MenuRow r is item n + r.
typedef enum MenuRow {
  kMenuRowVMManager = 0,
  kMenuRowUSB,
  kMenuRowSettings,
  kMenuRowCount,
} MenuRow;

static const UIBoxItem kVMManagerRow = {"Manage VMs", true};
static const UIBoxItem kUSBRow = {"USB mass storage", true};
static const UIBoxItem kSettingsRow = {"Settings", true};

extern const Screen kLauncherMenuScreen;

// The launcher's, given to LauncherMenuScreenOpen(): the card's VMs and
// settings, the guest RAM copies go through, and where to write why the
// menu closed.
static VMList* g_vms;
static Settings* g_settings;
static uint8_t* g_copy_buffer;
static uint32_t g_copy_buffer_size;
static LauncherMenuExit* g_exit;
// The highlighted item, and whether the menu has changed since it was last
// drawn.
static uint8_t g_highlighted;
static bool g_dirty;
// The row shown at the top of the box, past which the menu has scrolled.
static uint8_t g_top_row;
// The countdown: when the highlighted VM starts by itself, whether it is
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

// Scroll the menu, if it has to, so the highlighted item's row shows.
static void ScrollToHighlighted(void) {
  uint8_t row = g_highlighted;
  if (g_vms->count > 0 && g_highlighted >= g_vms->count) {
    ++row;
  }
  UIPageScrollTo(row, &g_top_row);
}

// Highlight item, scrolling the menu to it if it must.
static void Highlight(uint8_t item) {
  g_highlighted = item;
  g_dirty = true;
  ScrollToHighlighted();
}

ScreenResult LauncherMenuScreenOpen(
    VMList* vms, Settings* settings, uint8_t* copy_buffer,
    uint32_t copy_buffer_size, uint8_t highlighted, uint32_t countdown_ms,
    LauncherMenuExit* exit) {
  g_vms = vms;
  g_settings = settings;
  g_copy_buffer = copy_buffer;
  g_copy_buffer_size = copy_buffer_size;
  g_exit = exit;
  g_top_row = 0;
  Highlight(highlighted);
  g_start_at = make_timeout_time_ms(countdown_ms);
  g_counting = countdown_ms > 0;
  return ScreenOpen(&kLauncherMenuScreen);
}

// Show the seconds left before the highlighted VM starts by itself on the
// bottom edge, or with 0, nothing there.
static void SetCountdown(uint8_t seconds) {
  if (seconds != g_countdown) {
    g_countdown = seconds;
    g_dirty = true;
  }
}

// Close the menu to start the highlighted VM.
static ScreenResult StartHighlighted(void) {
  *g_exit = (LauncherMenuExit){kLauncherMenuExitStartVM, g_highlighted};
  return ScreenClose();
}

// Act on the highlighted item: start its VM, or open its row's screen over
// the menu.
static ScreenResult Choose(void) {
  if (g_highlighted < g_vms->count) {
    return StartHighlighted();
  }
  g_opened = (MenuRow)(g_highlighted - g_vms->count);
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

// Any key stops the countdown. Up and Down move the highlight, and Enter or
// Space chooses.
static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  g_counting = false;
  SetCountdown(0);
  // Esc and Backspace do nothing: the menu is the first screen, with nothing
  // under it to go back to.
  if (event.key == kKeyCodeEsc || event.key == kKeyCodeBackspace) {
    return ScreenStays();
  }
  if (event.state != kSouthbridgeKeyReleased && UIStatusClearProblem()) {
    g_dirty = true;
  }
  // A key held down repeats, which moves through the items as a press does.
  if (event.state == kSouthbridgeKeyReleased) {
    return ScreenStays();
  }
  if (UIPageIsMoveKey(event.key)) {
    if (UIPageMove(event.key, ItemCount(), &g_highlighted)) {
      g_dirty = true;
    }
    ScrollToHighlighted();
    return ScreenStays();
  }
  switch (event.key) {
    case kKeyCodeEnter:
    case kKeyCodeSpace:
      return Choose();
    default:
      return ScreenStays();
  }
}

// Run the countdown, showing the seconds left, and start the highlighted VM
// once they run out.
static ScreenResult CountDown(void) {
  if (!g_counting) {
    return ScreenStays();
  }
  const int64_t left_us =
      absolute_time_diff_us(get_absolute_time(), g_start_at);
  if (left_us <= 0) {
    g_counting = false;
    SetCountdown(0);
    return StartHighlighted();
  }
  SetCountdown((uint8_t)((left_us + kMicrosecondsPerSecond - 1) /
                         kMicrosecondsPerSecond));
  return ScreenStays();
}

// A screen opened over the menu has closed. After USB mass storage gave the
// card to the host, close the menu, for the launcher to read the card again.
// Otherwise highlight the row that opened the screen again: the VM manager
// may have changed how many VMs there are, which moves it.
static ScreenResult Resume(void) {
  if (g_exported) {
    g_exported = false;
    *g_exit = (LauncherMenuExit){kLauncherMenuExitReadCard, 0};
    return ScreenClose();
  }
  Highlight(RowItem(g_opened));
  return ScreenStays();
}

// Draw the rows that fit in the box from g_top_row, with arrows on its border
// where there are more rows above or below, and the countdown on its bottom
// edge.
static void Draw(Y_UNUSED uint64_t now_us) {
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
    const uint8_t index = (uint8_t)(g_top_row + i);
    if (index >= RowCount()) {
      UIBoxBlankRow(row, menu->columns);
    } else if (index < g_vms->count) {
      UIBoxItemRow(
          row, menu->columns, g_vms->vms[index].name, index == g_highlighted);
    } else if (g_vms->count > 0 && index == g_vms->count) {
      UIBoxSeparatorRow(row, menu->columns);
    } else {
      const uint8_t item = g_vms->count > 0 ? (uint8_t)(index - 1) : index;
      const bool highlighted = item == g_highlighted;
      if (item == RowItem(kMenuRowVMManager)) {
        UIBoxMenuItemRow(row, menu->columns, &kVMManagerRow, highlighted);
      } else if (item == RowItem(kMenuRowUSB)) {
        UIBoxMenuItemRow(row, menu->columns, &kUSBRow, highlighted);
      } else {
        UIBoxMenuItemRow(row, menu->columns, &kSettingsRow, highlighted);
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
  UIPageDrawScrollArrows(menu, g_top_row, RowCount());
  DisplayEndMenu();
  g_dirty = false;
}

static const char* Hint(void) { return "\x18\x19=Move  Enter=Select"; }

static const char* PageName(void) { return "vms"; }

const Screen kLauncherMenuScreen = {
    .handle_key = HandleKey,
    .tick = CountDown,
    .draw = Draw,
    .resume = Resume,
    .hint = Hint,
    .page_name = PageName,
};
