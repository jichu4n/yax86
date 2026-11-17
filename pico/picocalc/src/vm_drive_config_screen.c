#include "vm_drive_config_screen.h"

#include <stddef.h>
#include <stdio.h>

#include "box.h"
#include "browser_screen.h"
#include "core/fdc.h"
#include "core/hdc.h"
#include "disk_image.h"
#include "display.h"
#include "ff.h"
#include "file_browser.h"
#include "list_box.h"
#include "status_bar.h"
#include "text.h"
#include "vm_config.h"
#include "vm_floppy_type_screen.h"

// A drive's choices.
typedef enum DriveItem {
  // Left out of the machine: a hard disk. Floppy disk 2 is left out with
  // the drive type Disabled.
  kDriveDisabled = 0,
  // A floppy drive's mechanism type: 360KB, 720KB, 1.2MB or 1.44MB, or
  // Disabled for floppy disk 2.
  kDriveFloppyType,
  // A floppy drive with no disk in it.
  kDriveNone,
  kDriveBlank360KB,
  kDriveBlank720KB,
  kDriveBlank1200KB,
  kDriveBlank1440KB,
  kDriveBlank10MB,
  kDriveFloppyImage,
  kDriveHardDiskImage,
  kNumDriveItems,
} DriveItem;

static const BoxItem kDriveItems[kNumDriveItems] = {
    [kDriveDisabled] = {"Disabled", false},
    [kDriveFloppyType] = {"Drive type", true},
    [kDriveNone] = {"Empty", false},
    [kDriveBlank360KB] = {"New floppy image (360KB)", false},
    [kDriveBlank720KB] = {"New floppy image (720KB)", false},
    [kDriveBlank1200KB] = {"New floppy image (1.2MB)", false},
    [kDriveBlank1440KB] = {"New floppy image (1.44MB)", false},
    [kDriveBlank10MB] = {"New disk image (10MB)", false},
    [kDriveFloppyImage] = {"Existing floppy image", true},
    [kDriveHardDiskImage] = {"Existing disk image", true},
};

static const DriveItem kBlankFloppyItems[Y_FDC_NUM_FORMATS] = {
    kDriveBlank360KB,
    kDriveBlank720KB,
    kDriveBlank1200KB,
    kDriveBlank1440KB,
};

static const DriveItem kHardDiskChoices[] = {
    kDriveDisabled, kDriveBlank10MB, kDriveHardDiskImage};

extern const Screen kVMDriveConfigScreen;

// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;
// The drive, and the form's choice for it.
static uint8_t g_drive;
static VMDriveChoice* g_choice;
// Whether the screen opened over this one is the drive type choice.
static bool g_is_choosing_type;
// The image found in the browser, empty for none, and its size.
static char g_found_path[kFSMaxPath + 1];
static uint32_t g_found_size;

// The choices the drive offers, and how many: a floppy drive's type first,
// then the disk in it, with blank floppy choices following the type - and
// floppy disk 2 left out, with the type Disabled, has only the type; and a hard
// disk is there only with an image - with none there is no disk, rather than
// a drive with nothing in it.
static const DriveItem* Choices(uint8_t* num_choices) {
  if (g_drive >= kVMDriveHD1) {
    *num_choices = sizeof(kHardDiskChoices) / sizeof(kHardDiskChoices[0]);
    return kHardDiskChoices;
  }
  static DriveItem floppy_choices[kNumDriveItems];
  uint8_t count = 0;
  floppy_choices[count++] = kDriveFloppyType;
  if (!g_choice->is_present) {
    *num_choices = count;
    return floppy_choices;
  }
  floppy_choices[count++] = kDriveNone;
  for (uint8_t i = 0; i < Y_FDC_NUM_FORMATS; ++i) {
    if (YFDCDoesDriveTypeSupportFormat(g_choice->floppy_type, YFDCFormats[i])) {
      floppy_choices[count++] = kBlankFloppyItems[i];
    }
  }
  floppy_choices[count++] = kDriveFloppyImage;
  *num_choices = count;
  return floppy_choices;
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
    case kDriveBlank1200KB:
      return Y_FDC_FORMAT_1200KB_IMAGE_SIZE;
    case kDriveBlank1440KB:
      return Y_FDC_FORMAT_1440KB_IMAGE_SIZE;
    case kDriveBlank10MB:
      return Y_HDC_GEOMETRY_10MB_IMAGE_SIZE;
    default:
      return 0;
  }
}

ScreenResult VMDriveConfigScreenOpen(uint8_t drive, VMDriveChoice* selected) {
  g_drive = drive;
  g_choice = selected;
  g_is_choosing_type = false;
  DriveItem item = kDriveNone;
  if (!selected->is_present) {
    item = drive >= kVMDriveHD1 ? kDriveDisabled : kDriveFloppyType;
  } else if (selected->blank_size == Y_FDC_FORMAT_360KB_IMAGE_SIZE) {
    item = kDriveBlank360KB;
  } else if (selected->blank_size == Y_FDC_FORMAT_720KB_IMAGE_SIZE) {
    item = kDriveBlank720KB;
  } else if (selected->blank_size == Y_FDC_FORMAT_1200KB_IMAGE_SIZE) {
    item = kDriveBlank1200KB;
  } else if (selected->blank_size == Y_FDC_FORMAT_1440KB_IMAGE_SIZE) {
    item = kDriveBlank1440KB;
  } else if (selected->blank_size == Y_HDC_GEOMETRY_10MB_IMAGE_SIZE) {
    item = kDriveBlank10MB;
  } else if (selected->path[0] != '\0') {
    item = drive >= kVMDriveHD1 ? kDriveHardDiskImage : kDriveFloppyImage;
  }
  g_list_box.selected = ChoiceRow(item);
  return ScreenOpen(&kVMDriveConfigScreen);
}

// Take the choice, back to the form; open the drive type choices; or for an
// image, find it in the browser, starting where the one selected is, if one
// is.
static ScreenResult OnSelect(void) {
  uint8_t num_choices = 0;
  const DriveItem choice = Choices(&num_choices)[g_list_box.selected];
  if (choice == kDriveFloppyType) {
    g_is_choosing_type = true;
    return VMFloppyTypeScreenOpen(
        &g_choice->floppy_type,
        g_drive == kVMDriveFD2 ? &g_choice->is_present : NULL);
  }
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

// Drop any blank or existing floppy image on g_choice that the new drive type
// does not accept.
static void ReconcileFloppyTypeChange(void) {
  if (g_choice->blank_size != 0 &&
      !YFDCDoesDriveTypeSupportFormat(
          g_choice->floppy_type, YFDCFindFormat(g_choice->blank_size))) {
    g_choice->blank_size = 0;
  }
  if (g_choice->path[0] != '\0') {
    FILINFO info;
    if (f_stat(g_choice->path, &info) != FR_OK ||
        !YFDCDoesDriveTypeSupportFormat(
            g_choice->floppy_type, YFDCFindFormat((uint32_t)info.fsize))) {
      g_choice->path[0] = '\0';
    }
  }
}

// Back from the drive type choices or the browser: with Disabled chosen, or a
// compatible image found, back on the form.
static ScreenResult OnResume(void) {
  if (g_is_choosing_type) {
    g_is_choosing_type = false;
    if (!g_choice->is_present) {
      g_choice->path[0] = '\0';
      g_choice->blank_size = 0;
      return ScreenBack();
    }
    ReconcileFloppyTypeChange();
    g_list_box.is_dirty = true;
    return ScreenStays();
  }
  if (g_found_path[0] != '\0') {
    if (g_drive < kNumVMFloppyDrives &&
        !YFDCDoesDriveTypeSupportFormat(
            g_choice->floppy_type, YFDCFindFormat(g_found_size))) {
      char name[kDiskImageNameLength + 1];
      DiskImageName(g_found_path, name);
      char message[kTextColumns + 1];
      snprintf(
          message, sizeof(message), "%s does not fit a %s drive", name,
          VMConfigFloppyDriveTypeLabel(g_choice->floppy_type));
      StatusBarSetProblem(message);
      return BrowserScreenReopen();
    }
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
  char type_row[kListBoxNarrowColumns];
  BoxItem items[kNumDriveItems];
  for (uint8_t row = 0; row < num_choices; ++row) {
    if (choices[row] == kDriveFloppyType) {
      snprintf(
          type_row, sizeof(type_row), "%-15s %s", "Drive type",
          g_choice->is_present
              ? VMConfigFloppyDriveTypeLabel(g_choice->floppy_type)
              : "Disabled");
      items[row] = (BoxItem){type_row, true};
    } else {
      items[row] = kDriveItems[choices[row]];
    }
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
