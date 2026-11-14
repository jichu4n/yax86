#include "emulator_menu_screen.h"

#include <stdbool.h>
#include <stddef.h>

#include "box.h"
#include "display.h"
#include "floppy_drive_screen.h"
#include "list_box.h"
#include "question_screen.h"

typedef enum MainItem {
  // The drives' items, in the order of FloppyDrive.
  kMainDriveA = 0,
  kMainDriveB,
  kMainStatus,
  kMainReset,
  // Shown only while there is a list of VMs to go back to.
  kMainPowerOff,
  kMainItemCount,
} MainItem;

static const BoxItem kMainItems[kMainItemCount] = {
    [kMainDriveA] = {"Floppy disk 1", true},
    [kMainDriveB] = {"Floppy disk 2", true},
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

// The caller's: the running VM, the status page's lines, and where to write
// what the emulator is to do once the menu closes.
static RunningVM* g_vm;
static const StatusScreenLine* g_status_lines;
static EmulatorMenuExit* g_exit;
// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;
// The answers of the Reset and Power off questions, which they write.
static bool g_reset_confirmed;
static bool g_power_off_confirmed;

ScreenResult EmulatorMenuScreenOpen(
    RunningVM* vm, const StatusScreenLine status_lines[kStatusScreenLines],
    EmulatorMenuExit* exit) {
  g_vm = vm;
  g_status_lines = status_lines;
  g_exit = exit;
  *g_exit = kEmulatorMenuExitResume;
  g_reset_confirmed = false;
  g_power_off_confirmed = false;
  g_list_box.selected = 0;
  g_list_box.dirty = true;
  return ScreenOpen(&kEmulatorMenuScreen);
}

// The items as they show: Floppy disk 2 only with a second floppy drive, and
// Power off only with a list to go back to. Returns how many.
static uint8_t ShownItems(MainItem items[kMainItemCount]) {
  uint8_t count = 0;
  for (uint8_t item = 0; item < kMainItemCount; ++item) {
    if ((item == kMainDriveB && RunningVMFloppyDrives(g_vm) < 2) ||
        (item == kMainPowerOff && !RunningVMCanPowerOff(g_vm))) {
      continue;
    }
    items[count++] = (MainItem)item;
  }
  return count;
}

static uint8_t ItemCount(void) {
  MainItem items[kMainItemCount];
  return ShownItems(items);
}

// The item on the row selected.
static MainItem SelectedItem(void) {
  MainItem items[kMainItemCount];
  const uint8_t count = ShownItems(items);
  return g_list_box.selected < count ? items[g_list_box.selected]
                                     : kMainItemCount;
}

// Open the screen of the item selected.
static ScreenResult OnSelect(void) {
  const MainItem item = SelectedItem();
  switch (item) {
    case kMainDriveA:
    case kMainDriveB:
      return FloppyDriveScreenOpen(g_vm, (FloppyDrive)(item - kMainDriveA));
    case kMainStatus:
      return StatusScreenOpen(g_status_lines);
    case kMainReset:
      return QuestionScreenOpen(&kResetQuestion, &g_reset_confirmed);
    case kMainPowerOff:
      return QuestionScreenOpen(&kPowerOffQuestion, &g_power_off_confirmed);
    case kMainItemCount:
      break;
  }
  return ScreenStays();
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxHandleKeyEvent(&g_list_box, event, ItemCount())) {
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
  if (g_reset_confirmed) {
    *g_exit = kEmulatorMenuExitReset;
    return ScreenClose();
  }
  if (g_power_off_confirmed) {
    *g_exit = kEmulatorMenuExitPowerOff;
    return ScreenClose();
  }
  g_list_box.dirty = true;
  return ScreenStays();
}

static bool OnDraw(DisplayBox* box, Y_UNUSED uint64_t now_us) {
  if (!ListBoxTakeDirty(&g_list_box)) {
    return false;
  }
  MainItem shown[kMainItemCount];
  const uint8_t count = ShownItems(shown);
  BoxItem items[kMainItemCount];
  for (uint8_t i = 0; i < count; ++i) {
    items[i] = kMainItems[shown[i]];
  }
  ListBoxDrawItems(
      box, "yax86", NULL, 0, items, count, g_list_box.selected, false);
  return true;
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return "main"; }

const Screen kEmulatorMenuScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .on_resume = OnResume,
    .hint = Hint,
    .page_name = PageName,
};
