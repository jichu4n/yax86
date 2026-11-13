#include "vm_drive_config_screen.h"

#include <stddef.h>

#include "browser_screen.h"
#include "display.h"
#include "list_box.h"
#include "ui_box.h"
#include "ui_browser.h"
#include "vm_config.h"

// A drive's choices.
typedef enum DriveItem {
  // Left out of the machine: floppy disk 2, or a hard disk.
  kDriveDisabled = 0,
  // A floppy drive with no disk in it.
  kDriveNone,
  kDriveBlank,
  kDriveImage,
  kDriveItemCount,
} DriveItem;

static const UIBoxItem kFloppyItems[kDriveItemCount] = {
    [kDriveDisabled] = {"Disabled", false},
    [kDriveNone] = {"Empty", false},
    [kDriveBlank] = {"New floppy image (360KB)", false},
    [kDriveImage] = {"Existing floppy image", true},
};

static const UIBoxItem kHardDiskItems[kDriveItemCount] = {
    [kDriveDisabled] = {"Disabled", false},
    [kDriveBlank] = {"New disk image (10MB)", false},
    [kDriveImage] = {"Existing disk image", true},
};

// The choices each drive offers, in order: floppy disk 1 is always there;
// floppy disk 2 can be left out too; and a hard disk is there only with an
// image - with none there is no disk, rather than a drive with nothing in it.
static const DriveItem kFloppy1Choices[] = {
    kDriveNone, kDriveBlank, kDriveImage};
static const DriveItem kFloppy2Choices[] = {
    kDriveDisabled, kDriveNone, kDriveBlank, kDriveImage};
static const DriveItem kHardDiskChoices[] = {
    kDriveDisabled, kDriveBlank, kDriveImage};

extern const Screen kVMDriveConfigScreen;

// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;
// The drive, and the form's choice for it.
static uint8_t g_drive;
static VMDriveChoice* g_choice;
// The image found in the browser, empty for none, and its size.
static char g_found_path[kFSMaxPath + 1];
static uint32_t g_found_size;

// The choices the drive offers, and how many.
static const DriveItem* Choices(uint8_t* count) {
  if (g_drive == kVMDriveFD1) {
    *count = sizeof(kFloppy1Choices) / sizeof(kFloppy1Choices[0]);
    return kFloppy1Choices;
  }
  if (g_drive == kVMDriveFD2) {
    *count = sizeof(kFloppy2Choices) / sizeof(kFloppy2Choices[0]);
    return kFloppy2Choices;
  }
  *count = sizeof(kHardDiskChoices) / sizeof(kHardDiskChoices[0]);
  return kHardDiskChoices;
}

// The row of a choice, or with kDriveItemCount, how many rows there are.
static uint8_t ChoiceRow(DriveItem choice) {
  uint8_t count = 0;
  const DriveItem* choices = Choices(&count);
  for (uint8_t row = 0; row < count; ++row) {
    if (choices[row] == choice) {
      return row;
    }
  }
  return choice == kDriveItemCount ? count : 0;
}

ScreenResult VMDriveConfigScreenOpen(uint8_t drive, VMDriveChoice* chosen) {
  g_drive = drive;
  g_choice = chosen;
  DriveItem item = kDriveNone;
  if (!chosen->present) {
    item = kDriveDisabled;
  } else if (chosen->blank) {
    item = kDriveBlank;
  } else if (chosen->path[0] != '\0') {
    item = kDriveImage;
  }
  g_list_box.selected = ChoiceRow(item);
  g_list_box.dirty = true;
  return ScreenOpen(&kVMDriveConfigScreen);
}

// Take the choice, back to the form; or for an image, find it in the
// browser, starting where the one chosen is, if one is.
static ScreenResult Choose(void) {
  uint8_t count = 0;
  const DriveItem choice = Choices(&count)[g_list_box.selected];
  if (choice == kDriveImage) {
    char folder[kFSMaxPath + 1];
    if (g_choice->path[0] != '\0') {
      FSFolderOf(g_choice->path, folder);
    } else {
      FSCopyPath("yax86", folder);
    }
    return BrowserScreenOpen(
        g_drive >= kVMDriveHD1 ? kUIBrowserHardDisks : kUIBrowserFloppies,
        folder, g_choice->path, g_found_path, &g_found_size);
  }
  g_choice->path[0] = '\0';
  g_choice->blank = choice == kDriveBlank;
  g_choice->present = choice != kDriveDisabled;
  return ScreenBack();
}

static ScreenResult HandleKey(SouthbridgeKeyEvent event) {
  switch (ListBoxTakeEvent(&g_list_box, event, ChoiceRow(kDriveItemCount))) {
    case kListBoxKeySelect:
      return Choose();
    case kListBoxKeyBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

// Back from the browser: with an image found, it is the drive's, back on
// the form.
static ScreenResult Resume(void) {
  if (g_found_path[0] != '\0') {
    FSCopyPath(g_found_path, g_choice->path);
    g_choice->blank = false;
    g_choice->present = true;
    return ScreenBack();
  }
  g_list_box.dirty = true;
  return ScreenStays();
}

static void Draw(Y_UNUSED uint64_t now_us) {
  DisplayMenu* menu = ListBoxBeginDraw(&g_list_box);
  if (menu == NULL) {
    return;
  }
  uint8_t count = 0;
  const DriveItem* choices = Choices(&count);
  const UIBoxItem* labels =
      g_drive >= kVMDriveHD1 ? kHardDiskItems : kFloppyItems;
  UIBoxItem items[kDriveItemCount];
  for (uint8_t row = 0; row < count; ++row) {
    items[row] = labels[choices[row]];
  }
  ListBoxDrawItems(
      menu, kVMDriveNames[g_drive], NULL, 0, items, count, g_list_box.selected,
      true);
  ListBoxEndDraw(&g_list_box);
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return "vm_drive_config"; }

const Screen kVMDriveConfigScreen = {
    .handle_key = HandleKey,
    .draw = Draw,
    .resume = Resume,
    .hint = Hint,
    .page_name = PageName,
};
