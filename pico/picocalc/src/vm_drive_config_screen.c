#include "vm_drive_config_screen.h"

#include <stddef.h>

#include "box.h"
#include "browser_screen.h"
#include "core/fdc.h"
#include "core/hdc.h"
#include "display.h"
#include "file_browser.h"
#include "list_box.h"
#include "vm_config.h"

// A drive's choices.
typedef enum DriveItem {
  // Left out of the machine: floppy disk 2, or a hard disk.
  kDriveDisabled = 0,
  // A floppy drive with no disk in it.
  kDriveNone,
  kDriveBlank360KB,
  kDriveBlank720KB,
  kDriveBlank10MB,
  kDriveFloppyImage,
  kDriveHardDiskImage,
  kNumDriveItems,
} DriveItem;

static const BoxItem kDriveItems[kNumDriveItems] = {
    [kDriveDisabled] = {"Disabled", false},
    [kDriveNone] = {"Empty", false},
    [kDriveBlank360KB] = {"New floppy image (360KB)", false},
    [kDriveBlank720KB] = {"New floppy image (720KB)", false},
    [kDriveBlank10MB] = {"New disk image (10MB)", false},
    [kDriveFloppyImage] = {"Existing floppy image", true},
    [kDriveHardDiskImage] = {"Existing disk image", true},
};

// The choices each drive offers, in order: floppy disk 1 is always there;
// floppy disk 2 can be left out too; and a hard disk is there only with an
// image - with none there is no disk, rather than a drive with nothing in it.
static const DriveItem kFloppy1Choices[] = {
    kDriveNone, kDriveBlank360KB, kDriveBlank720KB, kDriveFloppyImage};
static const DriveItem kFloppy2Choices[] = {
    kDriveDisabled, kDriveNone, kDriveBlank360KB, kDriveBlank720KB,
    kDriveFloppyImage};
static const DriveItem kHardDiskChoices[] = {
    kDriveDisabled, kDriveBlank10MB, kDriveHardDiskImage};

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
static const DriveItem* Choices(uint8_t* num_choices) {
  if (g_drive == kVMDriveFD1) {
    *num_choices = sizeof(kFloppy1Choices) / sizeof(kFloppy1Choices[0]);
    return kFloppy1Choices;
  }
  if (g_drive == kVMDriveFD2) {
    *num_choices = sizeof(kFloppy2Choices) / sizeof(kFloppy2Choices[0]);
    return kFloppy2Choices;
  }
  *num_choices = sizeof(kHardDiskChoices) / sizeof(kHardDiskChoices[0]);
  return kHardDiskChoices;
}

// The row of a choice, or with kNumDriveItems, how many rows there are.
static uint8_t ChoiceRow(DriveItem choice) {
  uint8_t num_choices = 0;
  const DriveItem* choices = Choices(&num_choices);
  for (uint8_t row = 0; row < num_choices; ++row) {
    if (choices[row] == choice) {
      return row;
    }
  }
  return choice == kNumDriveItems ? num_choices : 0;
}

static uint32_t ChoiceBlankSize(DriveItem choice) {
  switch (choice) {
    case kDriveBlank360KB:
      return Y_FDC_FORMAT_360KB_IMAGE_SIZE;
    case kDriveBlank720KB:
      return Y_FDC_FORMAT_720KB_IMAGE_SIZE;
    case kDriveBlank10MB:
      return Y_HDC_GEOMETRY_10MB_IMAGE_SIZE;
    default:
      return 0;
  }
}

ScreenResult VMDriveConfigScreenOpen(uint8_t drive, VMDriveChoice* selected) {
  g_drive = drive;
  g_choice = selected;
  DriveItem item = kDriveNone;
  if (!selected->is_present) {
    item = kDriveDisabled;
  } else if (selected->blank_size == Y_FDC_FORMAT_360KB_IMAGE_SIZE) {
    item = kDriveBlank360KB;
  } else if (selected->blank_size == Y_FDC_FORMAT_720KB_IMAGE_SIZE) {
    item = kDriveBlank720KB;
  } else if (selected->blank_size == Y_HDC_GEOMETRY_10MB_IMAGE_SIZE) {
    item = kDriveBlank10MB;
  } else if (selected->path[0] != '\0') {
    item = drive >= kVMDriveHD1 ? kDriveHardDiskImage : kDriveFloppyImage;
  }
  g_list_box.selected = ChoiceRow(item);
  return ScreenOpen(&kVMDriveConfigScreen);
}

// Take the choice, back to the form; or for an image, find it in the
// browser, starting where the one selected is, if one is.
static ScreenResult OnSelect(void) {
  uint8_t num_choices = 0;
  const DriveItem choice = Choices(&num_choices)[g_list_box.selected];
  if (choice == kDriveFloppyImage || choice == kDriveHardDiskImage) {
    char folder[kFSMaxPath + 1];
    if (g_choice->path[0] != '\0') {
      FSFolderOf(g_choice->path, folder);
    } else {
      FSCopyPath("yax86", folder);
    }
    return BrowserScreenOpen(
        g_drive >= kVMDriveHD1 ? kFileBrowserHardDisks : kFileBrowserFloppies,
        folder, g_choice->path, NULL, g_found_path, &g_found_size);
  }
  g_choice->path[0] = '\0';
  g_choice->blank_size = ChoiceBlankSize(choice);
  g_choice->is_present = choice != kDriveDisabled;
  return ScreenBack();
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (
      ListBoxHandleKeyEvent(&g_list_box, event, ChoiceRow(kNumDriveItems))) {
    case kListBoxActionSelect:
      return OnSelect();
    case kListBoxActionBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

// Back from the browser: with an image found, it is the drive's, back on
// the form.
static ScreenResult OnResume(void) {
  if (g_found_path[0] != '\0') {
    FSCopyPath(g_found_path, g_choice->path);
    g_choice->blank_size = 0;
    g_choice->is_present = true;
    return ScreenBack();
  }
  return ScreenStays();
}

static void OnDraw(DisplayBox* box) {
  uint8_t num_choices = 0;
  const DriveItem* choices = Choices(&num_choices);
  BoxItem items[kNumDriveItems];
  for (uint8_t row = 0; row < num_choices; ++row) {
    items[row] = kDriveItems[choices[row]];
  }
  ListBoxDrawItems(
      box, kVMDriveNames[g_drive], NULL, 0, items, num_choices,
      g_list_box.selected, true);
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return "vm_drive_config"; }

const Screen kVMDriveConfigScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .on_resume = OnResume,
    .hint = Hint,
    .page_name = PageName,
    .is_dirty = &g_list_box.is_dirty,
};
