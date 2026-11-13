#include "launcher_menu_screen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "display.h"
#include "pico/stdlib.h"
#include "settings_screen.h"
#include "status_bar.h"
#include "ui_box.h"
#include "ui_page.h"
#include "usb_storage_screen.h"
#include "vm_manager_screen.h"

enum {
  kMicrosecondsPerSecond = 1000 * 1000,
};

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

extern const Screen kLauncherMenuScreen;

// The launcher's: the VMs and settings on the card, the RAM a copy goes
// through, and where the VM chosen goes.
static VMList* g_vms;
static Settings* g_settings;
static uint8_t* g_copy_buffer;
static uint32_t g_copy_buffer_size;
static LauncherMenuExit* g_exit;
static uint8_t g_chosen_item;
// Whether what is on the panel is out of date.
static bool g_dirty;
// The row at the top of the list.
static uint8_t g_list_top;
// When the default VM starts by itself, whether it is still counting down
// to it - a key stops it - and the seconds left that show.
static absolute_time_t g_start_at;
static bool g_counting;
static uint8_t g_countdown;
// The row that opened the screen showing over the list.
static ListRow g_opened;
// Whether the card or an image has been on USB, so the card is read again.
static bool g_exported;

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
  uint8_t row = g_chosen_item;
  if (g_vms->count > 0 && g_chosen_item >= g_vms->count) {
    ++row;
  }
  UIPageScrollTo(row, &g_list_top);
}

static void Show(uint8_t chosen) {
  g_chosen_item = chosen;
  g_dirty = true;
  ScrollToChosen();
}

// Show the list from its top with the item at highlighted chosen.
static void ShowFromTop(uint8_t highlighted) {
  g_list_top = 0;
  Show(highlighted);
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
  ShowFromTop(highlighted);
  g_start_at = make_timeout_time_ms(countdown_ms);
  g_counting = countdown_ms > 0;
  return ScreenOpen(&kLauncherMenuScreen);
}

// Show the seconds left before the highlighted VM starts by itself, or
// stop showing them with 0.
static void SetCountdown(uint8_t seconds) {
  if (seconds != g_countdown) {
    g_countdown = seconds;
    g_dirty = true;
  }
}

// Close the list to start the VM chosen, or highlighted.
static ScreenResult StartChosen(void) {
  *g_exit = (LauncherMenuExit){kLauncherMenuExitStartVM, g_chosen_item};
  return ScreenClose();
}

// Start the VM chosen, or open the screen of the row chosen over the list.
static ScreenResult Choose(void) {
  if (g_chosen_item < g_vms->count) {
    return StartChosen();
  }
  g_opened = (ListRow)(g_chosen_item - g_vms->count);
  switch (g_opened) {
    case kListRowManage:
      return VMManagerScreenOpen(
          g_vms, g_settings, g_copy_buffer, g_copy_buffer_size);
    case kListRowUSB:
      return USBStorageScreenOpen(&g_exported);
    default:
      return SettingsScreenOpen(g_settings);
  }
}

// A key pressed while the list shows: stops the countdown, and moves or
// chooses.
static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  g_counting = false;
  SetCountdown(0);
  // Nothing closes the list but starting something, and Esc goes back no
  // further than it.
  if (event.key == kKeyCodeEsc || event.key == kKeyCodeBackspace) {
    return ScreenStays();
  }
  if (event.state != kSouthbridgeKeyReleased && StatusBarClearProblem()) {
    g_dirty = true;
  }
  // A key held down repeats, which moves through the items as a press does.
  if (event.state == kSouthbridgeKeyReleased) {
    return ScreenStays();
  }
  if (UIPageIsMoveKey(event.key)) {
    if (UIPageMove(event.key, ItemCount(), &g_chosen_item)) {
      g_dirty = true;
    }
    ScrollToChosen();
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

// Count down to the default VM, showing the seconds left, which chooses it
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
    return StartChosen();
  }
  SetCountdown((uint8_t)((left_us + kMicrosecondsPerSecond - 1) /
                         kMicrosecondsPerSecond));
  return ScreenStays();
}

// Back from a screen opened over the list, with the row that opened it
// chosen, as the VMs may have changed under it; or after USB mass storage has
// exported the card or an image, closed for the card to be read again.
static ScreenResult Resume(void) {
  if (g_exported) {
    g_exported = false;
    *g_exit = (LauncherMenuExit){kLauncherMenuExitReadCard, 0};
    return ScreenClose();
  }
  Show(RowItem(g_opened));
  return ScreenStays();
}

// The VMs to start: a row each, with a line under them if there are any,
// arrows on the border where there are more above or below, and the
// countdown on the bottom edge.
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
    const uint8_t index = (uint8_t)(g_list_top + i);
    if (index >= RowCount()) {
      UIBoxBlankRow(row, menu->columns);
    } else if (index < g_vms->count) {
      UIBoxItemRow(
          row, menu->columns, g_vms->vms[index].name, index == g_chosen_item);
    } else if (g_vms->count > 0 && index == g_vms->count) {
      UIBoxSeparatorRow(row, menu->columns);
    } else {
      const uint8_t item = g_vms->count > 0 ? (uint8_t)(index - 1) : index;
      const bool chosen = item == g_chosen_item;
      if (item == RowItem(kListRowManage)) {
        UIBoxMenuItemRow(row, menu->columns, &kVMManagerRow, chosen);
      } else if (item == RowItem(kListRowUSB)) {
        UIBoxMenuItemRow(row, menu->columns, &kUSBRow, chosen);
      } else {
        UIBoxMenuItemRow(row, menu->columns, &kSettingsRow, chosen);
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
