#include "emulator_menu_screen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "box.h"
#include "display.h"
#include "floppy_drive_screen.h"
#include "list_box.h"
#include "question_screen.h"
#include "vm_speed_screen.h"

typedef enum MainItem {
  // The drives' items, in the order of FloppyDrive.
  kMainDriveA = 0,
  kMainDriveB,
  kMainSpeed,
  kMainStatus,
  kMainReset,
  // Shown only while there is a list of VMs to go back to.
  kMainPowerOff,
  kNumMainItems,
} MainItem;

static const BoxItem kMainItems[kNumMainItems] = {
    [kMainDriveA] = {"Floppy disk 1", true},
    [kMainDriveB] = {"Floppy disk 2", true},
    [kMainSpeed] = {"Speed", true},
    [kMainStatus] = {"Status", true},
    [kMainReset] = {"Reset", false},
    [kMainPowerOff] = {"Power off", false},
};

static const Question kResetQuestion = {
    .title = "Reset",
    .question = "Reset the VM?",
    .action = "Reset",
    .page_name = "reset",
};

static const Question kPowerOffQuestion = {
    .title = "Power off",
    .question = "Power off the VM?",
    .action = "Power off",
    .page_name = "power_off",
};

extern const Screen kEmulatorMenuScreen;

// The caller's: the emulator session, the status page's lines, and where to
// write what the emulator is to do once the menu closes.
static EmulatorSession* g_session;
static const StatusScreenLine* g_status_lines;
static EmulatorMenuExit* g_exit;
// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;
// The answers of the Reset, Power off and Speed screens, which they write.
static bool g_is_reset_confirmed;
static bool g_is_power_off_confirmed;
static VMSpeed g_speed_choice;
static bool g_is_choosing_speed;

ScreenResult EmulatorMenuScreenOpen(
    EmulatorSession* session,
    const StatusScreenLine status_lines[kStatusScreenNumLines],
    EmulatorMenuExit* exit) {
  g_session = session;
  g_status_lines = status_lines;
  g_exit = exit;
  *g_exit = kEmulatorMenuExitResume;
  g_is_reset_confirmed = false;
  g_is_power_off_confirmed = false;
  g_is_choosing_speed = false;
  g_speed_choice = EmulatorSessionSpeed(session);
  g_list_box.selected = 0;
  return ScreenOpen(&kEmulatorMenuScreen);
}

// The items as they show: Floppy disk 2 only with a second floppy drive, and
// Power off only with a list to go back to. Returns how many.
static uint8_t ShownItems(MainItem items[kNumMainItems]) {
  uint8_t num_shown = 0;
  for (uint8_t item = 0; item < kNumMainItems; ++item) {
    if ((item == kMainDriveB &&
         EmulatorSessionNumFloppyDrives(g_session) < 2) ||
        (item == kMainPowerOff && !EmulatorSessionCanPowerOff(g_session))) {
      continue;
    }
    items[num_shown++] = (MainItem)item;
  }
  return num_shown;
}

static uint8_t NumItems(void) {
  MainItem items[kNumMainItems];
  return ShownItems(items);
}

// The item on the row selected.
static MainItem SelectedItem(void) {
  MainItem items[kNumMainItems];
  const uint8_t num_shown = ShownItems(items);
  return g_list_box.selected < num_shown ? items[g_list_box.selected]
                                         : kNumMainItems;
}

// Open the screen of the item selected.
static ScreenResult OnSelect(void) {
  const MainItem item = SelectedItem();
  switch (item) {
    case kMainDriveA:
    case kMainDriveB:
      return FloppyDriveScreenOpen(
          g_session, (FloppyDrive)(item - kMainDriveA));
    case kMainSpeed:
      g_speed_choice = EmulatorSessionSpeed(g_session);
      g_is_choosing_speed = true;
      return VMSpeedScreenOpen(&g_speed_choice);
    case kMainStatus:
      return StatusScreenOpen(g_status_lines);
    case kMainReset:
      return QuestionScreenOpen(&kResetQuestion, &g_is_reset_confirmed);
    case kMainPowerOff:
      return QuestionScreenOpen(&kPowerOffQuestion, &g_is_power_off_confirmed);
    case kNumMainItems:
      break;
  }
  return ScreenStays();
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxHandleKeyEvent(&g_list_box, event, NumItems())) {
    case kListBoxActionSelect:
      return OnSelect();
    case kListBoxActionBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

// Back from another page, with the item that opened it still selected. A
// Reset or Power off answered yes closes the menu, for the emulator to carry
// it out.
static ScreenResult OnResume(void) {
  if (g_is_choosing_speed) {
    g_is_choosing_speed = false;
    if (g_speed_choice != EmulatorSessionSpeed(g_session)) {
      EmulatorSessionSetSpeed(g_session, g_speed_choice);
      return ScreenClose();
    }
    return ScreenStays();
  }
  if (g_is_reset_confirmed) {
    *g_exit = kEmulatorMenuExitReset;
    return ScreenClose();
  }
  if (g_is_power_off_confirmed) {
    *g_exit = kEmulatorMenuExitPowerOff;
    return ScreenClose();
  }
  return ScreenStays();
}

static void OnDraw(DisplayBox* box) {
  MainItem shown[kNumMainItems];
  const uint8_t num_shown = ShownItems(shown);
  char speed_row[kListBoxNarrowColumns];
  snprintf(
      speed_row, sizeof(speed_row), "Speed: %s",
      VMConfigSpeedLabel(EmulatorSessionSpeed(g_session)));
  BoxItem items[kNumMainItems];
  for (uint8_t i = 0; i < num_shown; ++i) {
    items[i] = shown[i] == kMainSpeed ? (BoxItem){speed_row, true}
                                      : kMainItems[shown[i]];
  }
  ListBoxDrawItems(
      box, "yax86", NULL, 0, items, num_shown, g_list_box.selected, false);
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return "main"; }

const Screen kEmulatorMenuScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .on_resume = OnResume,
    .hint = Hint,
    .page_name = PageName,
    .is_dirty = &g_list_box.is_dirty,
};
