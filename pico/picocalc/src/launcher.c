#include "launcher.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "disk_image.h"
#include "display.h"
#include "font_4x10.h"
#include "fs.h"
#include "hardware/watchdog.h"
#include "launcher_manage.h"
#include "launcher_settings.h"
#include "launcher_usb.h"
#include "pico/stdlib.h"
#include "runtime_log.h"
#include "status_bar.h"
#include "ui_box.h"
#include "ui_page.h"
#include "vm_folder.h"

enum {
  // Written to g_start_note's marker by a reset or a power off, for the
  // start after the reboot - see LeaveStartNote().
  kStartNoteMarker = 0x5747A27E,

  kMicrosecondsPerSecond = 1000 * 1000,
  kMillisecondsPerSecond = 1000,
};

// ============================================================================
// The list
// ============================================================================

// What a key did on the list.
typedef enum ListAction {
  // Nothing the caller need act on.
  kListActionNone = 0,
  // A VM was chosen from the list: see ListChosenVM().
  kListActionVMChosen,
  // A row under the VMs was chosen. Show the list again after what it opens
  // with ListShowManageRow(), ListShowUSBRow() or
  // ListShowSettingsRow().
  kListActionOpenManage,
  kListActionOpenUSB,
  kListActionOpenSettings,
} ListAction;

static const UIBoxItem kManageRow = {"Manage VMs", true};
static const UIBoxItem kUSBRow = {"USB disk mode", true};
static const UIBoxItem kSettingsRow = {"Settings", true};

// The VMs on the card, and the card's settings.
static VMList g_vms;
static VMSettings g_settings;

// Whether the list shows: from ListOpen() until ListClose().
static bool g_open;
static uint8_t g_chosen;
// Whether what is on the panel is out of date.
static bool g_dirty;
// The row at the top of the list.
static uint8_t g_list_top;
// The seconds before the highlighted VM starts by itself.
static uint8_t g_countdown;

static bool ListIsOpen(void) { return g_open; }

static const char* ListPageName(void) { return "vms"; }

static void ScrollToChosen(void);

static void ListOpen(uint8_t highlighted) {
  g_open = true;
  g_list_top = 0;
  g_chosen = highlighted;
  g_dirty = true;
  ScrollToChosen();
}

static uint8_t ListChosenVM(void) { return g_chosen; }

static void ListClose(void) { g_open = false; }

static void ListSetCountdown(uint8_t seconds) {
  if (seconds != g_countdown) {
    g_countdown = seconds;
    g_dirty = true;
  }
}

static const char* ListHint(void) { return "\x18\x19=Move  Enter=Select"; }

// Keep the chosen item among the list's rows.
static void ScrollToChosen(void) {
  uint8_t row = g_chosen;
  if (g_vms.count > 0 && g_chosen >= g_vms.count) {
    ++row;
  }
  UIPageScrollTo(row, &g_list_top);
}

// The list at power on's items: a VM each, then Manage VMs, USB disk mode and
// Settings, with a separator row between the VMs and the rest when there is at
// least one VM.
static uint8_t VMsItemCount(void) { return (uint8_t)(g_vms.count + 3); }

static uint8_t VMsRowCount(void) {
  return (uint8_t)(g_vms.count + (g_vms.count > 0 ? 4 : 3));
}

static uint8_t ManageItemIndex(void) { return g_vms.count; }

static uint8_t USBItemIndex(void) { return (uint8_t)(g_vms.count + 1); }

static uint8_t SettingsItemIndex(void) { return (uint8_t)(g_vms.count + 2); }

// Show the list again with an item chosen, when what was opened from it is
// left.
static void ShowItem(uint8_t item) {
  g_chosen = item;
  g_dirty = true;
  ScrollToChosen();
}

static void ListShowManageRow(void) { ShowItem(ManageItemIndex()); }

static void ListShowUSBRow(void) { ShowItem(USBItemIndex()); }

static void ListShowSettingsRow(void) { ShowItem(SettingsItemIndex()); }

static ListAction Choose(void) {
  if (g_chosen == SettingsItemIndex()) {
    return kListActionOpenSettings;
  }
  if (g_chosen == USBItemIndex()) {
    return kListActionOpenUSB;
  }
  if (g_chosen == ManageItemIndex()) {
    return kListActionOpenManage;
  }
  return kListActionVMChosen;
}

static ListAction ListHandleKey(SouthbridgeKeyEvent event) {
  // Nothing closes the list but starting something, and Esc goes back no
  // further than it.
  if (event.key == kKeyCodeEsc || event.key == kKeyCodeBackspace) {
    return kListActionNone;
  }
  if (event.state != kSouthbridgeKeyReleased && LauncherClearProblem()) {
    g_dirty = true;
  }
  // A key held down repeats, which moves through the items as a press does.
  if (event.state == kSouthbridgeKeyReleased) {
    return kListActionNone;
  }
  if (UIPageIsMoveKey(event.key)) {
    if (UIPageMove(event.key, VMsItemCount(), &g_chosen)) {
      g_dirty = true;
    }
    ScrollToChosen();
    return kListActionNone;
  }
  if (event.key == kKeyCodeEnter || event.key == kKeyCodeSpace) {
    return Choose();
  }
  return kListActionNone;
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
    } else if (index < g_vms.count) {
      UIBoxItemRow(
          row, menu->columns, g_vms.vms[index].name, index == g_chosen);
    } else if (g_vms.count > 0 && index == g_vms.count) {
      UIBoxSeparatorRow(row, menu->columns);
    } else {
      const uint8_t item = g_vms.count > 0 ? (uint8_t)(index - 1) : index;
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

static void ListDraw(void) {
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

// ============================================================================
// Choosing a VM
// ============================================================================

typedef struct StartNote {
  // kStartNoteMarker while the note is to be acted on.
  uint32_t marker;
  StartIntent intent;
  // The VM's name.
  char name[kVMNameLength + 1];
} StartNote;

// Deliberately not zeroed at startup, so that it survives the reboot - see
// LeaveStartNote().
static StartNote __uninitialized_ram(g_start_note);

// When the default VM starts by itself, and whether it is still counting
// down to it: a key stops it.
static absolute_time_t g_start_at;
static bool g_counting;
// What is open from the list, if anything.
typedef enum Feature {
  kFeatureNone = 0,
  kFeatureManage,
  kFeatureUsb,
  kFeatureSettings,
} Feature;
static Feature g_feature;
// Why something on the list could not be done, shown in place of the keys
// until another key is pressed. It is kept in two buffers, used in turn, so
// that a new one is a new pointer: the status rows tell a change of hint by
// its pointer.
static char g_problems[2][kTextColumns + 1];
static char* g_problem = g_problems[0];

void LauncherSetProblem(const char* problem) {
  g_problem = g_problem == g_problems[0] ? g_problems[1] : g_problems[0];
  snprintf(g_problem, kTextColumns + 1, "%s", problem != NULL ? problem : "");
}

bool LauncherClearProblem(void) {
  if (g_problem[0] == '\0') {
    return false;
  }
  g_problem[0] = '\0';
  return true;
}

// Restart the board, leaving a note for the start after it, which takes it
// - see TakeStartNote(). Every start, whether from power on, a reset or a power
// off, begins from the same state. Nothing is lost: the keyboard controller
// keeps its power, and the guest's disks have every sector it wrote on the
// card.
//
// The note is in RAM that startup does not clear, as g_overclock_marker is: it
// survives the reboot but not a power cycle. It is only ever set across the
// reboot, and the start after takes it whatever it does next, so a later
// reboot - picotool's, or the power button's - never finds one left from
// before.
void LauncherLeaveStartNote(StartIntent intent, const char* name) {
  g_start_note.intent = intent;
  memcpy(g_start_note.name, name, sizeof(g_start_note.name));
  g_start_note.marker = kStartNoteMarker;
  watchdog_reboot(0, 0, 0);
  for (;;) {
    tight_loop_contents();
  }
}

// Take the note a reset or power off left, if one did: returns whether there
// was one.
static bool TakeStartNote(StartNote* note) {
  if (g_start_note.marker != kStartNoteMarker) {
    return false;
  }
  g_start_note.marker = 0;
  note->intent = g_start_note.intent;
  // The name may have been left unterminated by RAM it never wrote.
  g_start_note.name[kVMNameLength] = '\0';
  memcpy(note->name, g_start_note.name, sizeof(note->name));
  return true;
}

// Make the VM named name the one to start at the next power on, if it is not
// already. Writing the card only when it changes keeps a board that starts
// its default every time from writing at all.
static void SaveBootDefault(const char* name) {
  if (strcasecmp(g_settings.boot_default, name) == 0) {
    return;
  }
  char problem[kVMProblemLength + 1];
  if (VMSettingsSaveBootDefault(name, problem)) {
    snprintf(
        g_settings.boot_default, sizeof(g_settings.boot_default), "%s", name);
    SYSTEM_INFO("boot_default_saved", Y_LOG_STRING("name", name));
    return;
  }
  char message[kTextColumns + 1];
  snprintf(
      message, sizeof(message), "Could not save boot_default: %.50s", problem);
  StatusBarPostMessage(message);
  SYSTEM_WARN("boot_default_not_saved", Y_LOG_STRING("problem", problem));
}

static __noinline bool TakeVM(uint8_t index, VMConfig* chosen) {
  const VMEntry* vm = &g_vms.vms[index];
  char problem[kVMProblemLength + 1];
  if (VMConfigRead(vm, chosen, problem)) {
    SYSTEM_INFO("vm_chosen", Y_LOG_STRING("path", vm->path));
    SaveBootDefault(vm->name);
    return true;
  }
  char message[kTextColumns + 1];
  snprintf(message, sizeof(message), "%s: %s", vm->name, problem);
  LauncherSetProblem(message);
  SYSTEM_WARN(
      "vm_config_problem", Y_LOG_STRING("path", vm->path),
      Y_LOG_STRING("problem", problem));
  return false;
}

// The card's settings, and the index of the default VM in the list.
// Returns false, with the problem shown on the list, if the settings had one.
static bool ReadSettings(VMSettings* settings, uint8_t* highlighted) {
  char problem[kVMProblemLength + 1];
  bool read = VMSettingsRead(settings, problem);
  DisplaySetFont(settings->font);
  *highlighted = 0;
  if (read && settings->boot_default[0] != '\0') {
    // Past the list's end, the first is highlighted instead.
    const uint8_t index = VMListFind(&g_vms, settings->boot_default);
    if (index < kVMListMax) {
      *highlighted = index;
    }
  }
  if (!read) {
    char message[kTextColumns + 1];
    snprintf(message, sizeof(message), "YAX86.CFG: %s", problem);
    LauncherSetProblem(message);
    SYSTEM_WARN("settings_problem", Y_LOG_STRING("problem", problem));
  }
  return read;
}

void LauncherUpdateRows(void) {
  StatusBarState state = {0};
  state.battery = SouthbridgeGetBattery();
  state.hint = g_feature == kFeatureManage     ? LauncherManageHint()
               : g_feature == kFeatureUsb      ? LauncherUsbHint()
               : g_feature == kFeatureSettings ? LauncherSettingsHint()
                                               : ListHint();
  state.problem = g_problem[0] != '\0' ? g_problem : NULL;
  StatusBarOffer(&state);
}

void LauncherInit(uint8_t* copy_buffer) {
  LauncherManageInit(&g_vms, copy_buffer);
}

void LauncherReadCard(void) {
  VMListRead(&g_vms);
  uint8_t highlighted = 0;
  ReadSettings(&g_settings, &highlighted);
}

const VMList* LauncherVMs(void) { return &g_vms; }

const VMSettings* LauncherSettings(void) { return &g_settings; }

LauncherStartResult LauncherStart(VMConfig* vm) {
  StartNote note = {0};
  const bool noted = TakeStartNote(&note);
  VMListRead(&g_vms);
  uint8_t highlighted = 0;
  const bool settings_read = ReadSettings(&g_settings, &highlighted);
  // Without a card there is nothing to choose, or to put on USB.
  if (DiskImageMountCard() != kDiskImageOpen) {
    return kLauncherNoCard;
  }
  if (noted) {
    SYSTEM_INFO(
        note.intent == kStartIntentRestart ? "restarting" : "powered_off",
        Y_LOG_STRING("name", note.name));
    const uint8_t index = VMListFind(&g_vms, note.name);
    if (index < kVMListMax) {
      highlighted = index;
      if (note.intent == kStartIntentRestart && TakeVM(index, vm)) {
        return kLauncherTookVM;
      }
    }
  } else if (
      settings_read && g_settings.boot_delay == 0 && g_vms.count > 0 &&
      TakeVM(highlighted, vm)) {
    return kLauncherTookVM;
  }
  ListOpen(highlighted);
  g_start_at =
      make_timeout_time_ms(g_settings.boot_delay * kMillisecondsPerSecond);
  g_counting = !noted && g_settings.boot_delay > 0 && g_vms.count > 0;
  return kLauncherShowsList;
}

// A key while USB disk mode is open: back to the list when it is left, and
// the card read afresh when it is back from USB.
static void HandleUsbKey(SouthbridgeKeyEvent key) {
  switch (LauncherUsbHandleKey(key)) {
    case kLauncherUsbStays:
      break;
    case kLauncherUsbLeft:
      g_feature = kFeatureNone;
      ListShowUSBRow();
      break;
    case kLauncherUsbReturned: {
      g_feature = kFeatureNone;
      VMListRead(&g_vms);
      uint8_t highlighted = 0;
      ReadSettings(&g_settings, &highlighted);
      ListOpen(highlighted);
      break;
    }
  }
}

// A key while Settings is open: back to the list when it is left, and the
// settings kept as they are saved when another font is chosen.
static void HandleSettingsKey(SouthbridgeKeyEvent key) {
  switch (LauncherSettingsHandleKey(key)) {
    case kLauncherSettingsStays:
      break;
    case kLauncherSettingsLeft:
      g_feature = kFeatureNone;
      ListShowSettingsRow();
      break;
    case kLauncherSettingsFontChanged:
      g_settings.font = LauncherSettingsFont();
      break;
  }
}

bool LauncherHandleKey(SouthbridgeKeyEvent key) {
  g_counting = false;
  ListSetCountdown(0);
  if (g_feature == kFeatureManage) {
    if (LauncherManageHandleKey(key) == kLauncherManageLeft) {
      g_feature = kFeatureNone;
      ListShowManageRow();
    }
    return false;
  }
  if (g_feature == kFeatureUsb) {
    HandleUsbKey(key);
    return false;
  }
  if (g_feature == kFeatureSettings) {
    HandleSettingsKey(key);
    return false;
  }
  bool chosen = false;
  switch (ListHandleKey(key)) {
    case kListActionVMChosen:
      chosen = true;
      break;
    case kListActionOpenUSB:
      g_feature = kFeatureUsb;
      LauncherUsbOpen();
      break;
    case kListActionOpenSettings:
      g_feature = kFeatureSettings;
      LauncherSettingsOpen(g_settings.font);
      break;
    case kListActionOpenManage:
      g_feature = kFeatureManage;
      LauncherManageOpen();
      break;
    default:
      break;
  }
  return chosen;
}

bool LauncherCountDown(void) {
  bool chosen = false;
  if (g_counting) {
    const int64_t left_us =
        absolute_time_diff_us(get_absolute_time(), g_start_at);
    if (left_us <= 0) {
      g_counting = false;
      ListSetCountdown(0);
      chosen = true;
    } else {
      ListSetCountdown((uint8_t)((left_us + kMicrosecondsPerSecond - 1) /
                                 kMicrosecondsPerSecond));
    }
  }
  return chosen;
}

bool LauncherTakeChosen(VMConfig* vm) {
  if (!TakeVM(ListChosenVM(), vm)) {
    return false;
  }
  ListClose();
  return true;
}

bool LauncherIsOpen(void) { return ListIsOpen(); }

void LauncherDraw(uint64_t now_us) {
  switch (g_feature) {
    case kFeatureManage:
      LauncherManageDraw();
      break;
    case kFeatureUsb:
      LauncherUsbDraw(now_us);
      break;
    case kFeatureSettings:
      LauncherSettingsDraw();
      break;
    case kFeatureNone:
      ListDraw();
      break;
  }
}

const char* LauncherPageName(void) {
  switch (g_feature) {
    case kFeatureManage:
      return LauncherManagePageName();
    case kFeatureUsb:
      return LauncherUsbPageName();
    case kFeatureSettings:
      return LauncherSettingsPageName();
    case kFeatureNone:
      break;
  }
  return ListPageName();
}
