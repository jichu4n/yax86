#include "menu.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "display.h"
#include "file_list.h"
#include "harddisk.h"
#include "ui_box.h"
#include "usb_disk.h"
#include "vm_folder.h"

enum {
  // The controller's codes for the keys the menu listens to.
  kKeyLeftShift = 0xA2,
  kKeyRightShift = 0xA3,
  kKeyUp = 0xB5,
  kKeyDown = 0xB6,
  kKeyEnter = 0x0A,
  kKeySpace = ' ',
  kKeyEsc = 0xB1,
  kKeyBackspace = 0x08,
  kKeyPageUp = 0xD6,
  kKeyPageDown = 0xD7,

  // A floppy image's size: the browser lists only files this long.
  kFloppyImageSize = 360 * 1024,
  // The browser's rows of entries, which with its edges fill the most rows a
  // box can have.
  kBrowseRows = kDisplayMenuMaxRows - 2,
  // Where an entry's note starts on its row: a folder's <DIR>, and which
  // drive an image is in.
  kBrowseNoteColumn = 18,

  // A label's column on the status page.
  kStatusLabelColumns = 9,

  kStatusRefreshUs = 1000 * 1000,
};

typedef enum MenuPage {
  kMenuPageMain = 0,
  kMenuPageStatus,
  kMenuPageReset,
  kMenuPagePowerOff,
  // The page of the drive in g_drive.
  kMenuPageDrive,
  kMenuPageBrowse,
  // The VMs to start, at power on, and from there what can go on USB, the
  // card while it is there, and the question before taking it back.
  kMenuPageVMs,
  kMenuPageUSB,
  kMenuPageSharing,
  kMenuPageStopSharing,
  // Manage VMs, from the list: the VMs in yax86/vm, the page of the one in
  // g_vm, the name typed to rename or clone it as, the question before
  // deleting it, and a clone being copied.
  kMenuPageManage,
  kMenuPageVM,
  kMenuPageName,
  kMenuPageDelete,
  kMenuPageCopying,
  // New VM, from Manage VMs: its form, and the choice for one of its drives,
  // g_new_drive.
  kMenuPageNew,
  kMenuPageNewDrive,
  // Settings, from the list, and Font, from Settings: the 4x10 fonts.
  kMenuPageSettings,
  kMenuPageFont,
  kMenuPageCount,
} MenuPage;

// A menu page definition for table-driven dispatch.
typedef struct MenuPageDef {
  const char* name;
  const char* (*get_name)(void);
  const char* (*get_hint)(void);
  const char* hint;
  uint8_t (*item_count)(void);
  void (*draw)(DisplayMenu* menu);
  MenuAction (*choose)(void);
  MenuAction (*back)(void);
  bool (*handle_key)(SouthbridgeKeyEvent event, MenuAction* action);
} MenuPageDef;

static const MenuPageDef kPageDefs[kMenuPageCount];

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

static const MenuItem kMainItems[kMainItemCount] = {
    [kMainDriveA] = {"Floppy disk 1", true},
    [kMainDriveB] = {"Floppy disk 2", true},
    [kMainStatus] = {"Status", true},
    [kMainReset] = {"Reset", false},
    [kMainPowerOff] = {"Power off", false},
};

typedef enum DriveItem {
  kDriveInsert = 0,
  // Shown only while there is something to eject.
  kDriveEject,
  kDriveItemCount,
} DriveItem;

typedef enum ResetItem {
  kResetCancel = 0,
  kResetConfirm,
  kResetItemCount,
} ResetItem;

static const MenuItem kResetItems[kResetItemCount] = {
    [kResetCancel] = {"Cancel", false},
    [kResetConfirm] = {"Reset", false},
};

typedef enum PowerOffItem {
  kPowerOffCancel = 0,
  kPowerOffConfirm,
  kPowerOffItemCount,
} PowerOffItem;

static const MenuItem kPowerOffItems[kPowerOffItemCount] = {
    [kPowerOffCancel] = {"Cancel", false},
    [kPowerOffConfirm] = {"Power off", false},
};

typedef enum USBItem {
  kUSBCard = 0,
  kUSBImage,
  kUSBItemCount,
} USBItem;

static const MenuItem kUSBItems[kUSBItemCount] = {
    [kUSBCard] = {"Whole SD card", false},
    [kUSBImage] = {"Disk image", true},
};

static const MenuItem kSharingItems[] = {{"Disconnect", false}};

typedef enum StopSharingItem {
  kStopSharingCancel = 0,
  kStopSharingConfirm,
  kStopSharingItemCount,
} StopSharingItem;

static const MenuItem kStopSharingItems[kStopSharingItemCount] = {
    [kStopSharingCancel] = {"Cancel", false},
    [kStopSharingConfirm] = {"Disconnect anyway", false},
};

typedef enum VMItem {
  kVMEdit = 0,
  kVMRename,
  kVMClone,
  kVMDelete,
  kVMItemCount,
} VMItem;

static const MenuItem kVMItems[kVMItemCount] = {
    [kVMEdit] = {"Edit drives", true},
    [kVMRename] = {"Rename", true},
    [kVMClone] = {"Clone", true},
    [kVMDelete] = {"Delete", false},
};

typedef enum DeleteItem {
  kDeleteCancel = 0,
  kDeleteConfirm,
  kDeleteItemCount,
} DeleteItem;

static const MenuItem kDeleteItems[kDeleteItemCount] = {
    [kDeleteCancel] = {"Cancel", false},
    [kDeleteConfirm] = {"Delete", false},
};

typedef enum NewItem {
  kNewName = 0,
  // The drives' items, in the order of the VM's drives.
  kNewDriveA,
  kNewDriveB,
  kNewDriveC,
  kNewDriveD,
  kNewCreate,
  kNewItemCount,
} NewItem;

// The choices for a new VM's drive: a blank floppy for a floppy drive, and a
// blank hard disk for drive C.
// A drive's choices on the form.
typedef enum NewDriveItem {
  // Left out of the machine: floppy disk 2, or a hard disk.
  kNewDriveDisabled = 0,
  // A floppy drive with no disk in it.
  kNewDriveNone,
  kNewDriveBlank,
  kNewDriveImage,
  kNewDriveItemCount,
} NewDriveItem;

static const MenuItem kNewFloppyItems[kNewDriveItemCount] = {
    [kNewDriveDisabled] = {"Disabled", false},
    [kNewDriveNone] = {"Empty", false},
    [kNewDriveBlank] = {"New floppy image (360KB)", false},
    [kNewDriveImage] = {"Existing floppy image", true},
};

static const MenuItem kNewHardDiskItems[kNewDriveItemCount] = {
    [kNewDriveDisabled] = {"Disabled", false},
    [kNewDriveBlank] = {"New disk image (10MB)", false},
    [kNewDriveImage] = {"Existing disk image", true},
};

// The choices each drive offers, in order: floppy disk 1 is always there;
// floppy disk 2 can be left out too; and a hard disk is there only with an
// image - with none there is no disk, rather than a drive with nothing in it.
static const NewDriveItem kFloppy1Choices[] = {
    kNewDriveNone, kNewDriveBlank, kNewDriveImage};
static const NewDriveItem kFloppy2Choices[] = {
    kNewDriveDisabled, kNewDriveNone, kNewDriveBlank, kNewDriveImage};
static const NewDriveItem kHardDiskChoices[] = {
    kNewDriveDisabled, kNewDriveBlank, kNewDriveImage};

static const MenuItem kNewVMRow = {"New VM", true};
static const MenuItem kManageRow = {"Manage VMs", true};
static const MenuItem kUSBRow = {"USB disk mode", true};
static const MenuItem kSettingsRow = {"Settings", true};
static const MenuItem kSettingsItems[] = {{"Font", true}};
enum {
  kSettingsItemCount = sizeof(kSettingsItems) / sizeof(kSettingsItems[0]),
};

static MenuConfig g_config;
static bool g_open;
static MenuPage g_page;
// The drive whose page or browser shows.
static FloppyDrive g_drive;
static uint8_t g_chosen;
// Whether what is on the panel is out of date, and when the status page was
// last drawn.
static bool g_dirty;
static uint64_t g_status_drawn_us;
// The box last drawn, whose size a new one has to match for nothing of it to
// be left showing, and whether there is one.
static uint8_t g_drawn_columns;
static uint8_t g_drawn_rows;
static bool g_box_drawn;
// The folder the browser shows, and the entry at the top of its rows.
static FileList g_list;
static uint8_t g_list_top;
// The VMs to choose from at power on, the seconds before the
// highlighted one starts by itself, and why the last chosen did not.
static const VMList* g_vms;
static uint8_t g_countdown;
static Font4x10 g_font = kFont4x10Default;
// Whether the menu is the list at power on, or a page reached from it, which
// only starting something closes.
static bool g_choosing;
// What the browser is finding an image for: a floppy for a drive, an image of
// either kind to put on USB, from the list, or an image for a new VM's drive.
typedef enum BrowsePurpose {
  kBrowseForDrive = 0,
  kBrowseForUSB,
  kBrowseForNewVM,
} BrowsePurpose;
static BrowsePurpose g_browsing_for;
// The image found to put on USB.
static char g_usb_image_path[kFSMaxPath + 1];
static uint32_t g_usb_image_size;
//
// The VM whose page shows, an index in g_vms; the names of the images in its
// drives, empty for none; and whether its vm.cfg read.
static uint8_t g_vm;
static char g_vm_drives[kVMDrives][kDiskImageNameLength + 1];
static bool g_vm_present[kVMDrives];
static bool g_vm_readable;
// The name being typed, and whether it is for a clone rather than a rename.
static char g_name[kFSMaxFolderName + 1];
static uint8_t g_name_length;
static bool g_cloning;
// What deleting the VM would take.
static VMFolderContents g_contents;
// How far a clone has been copied, or a new VM's hard disk written, in KB,
// and which it is.
static uint32_t g_copied_kb;
static uint32_t g_copy_kb;
static bool g_creating;
// A new VM: its name is g_name; the images in its floppy drives and drive C,
// paths from the card's root or empty for none; whether each is a blank disk
// hard disk of its own instead; and the drive whose choices show.
static char g_new_drives[kVMDrives][kFSMaxPath + 1];
static bool g_new_blank[kVMDrives];
// Whether each drive is there: false for floppy disk 2 disabled.
static bool g_new_present[kVMDrives];
static uint8_t g_new_drive;
// Whether the form is editing the drives of the VM in g_vm, rather than
// making a new one; it then has no name, and saves rather than creates.
static bool g_editing;
//
// The problem is kept in two buffers, used in turn, so that a new one is a
// new pointer: the status rows tell a change of hint by its pointer.
static char g_problems[2][kTextColumns + 1];
static char* g_problem = g_problems[0];
// Which Shift keys are held, from the controller's events.
static bool g_left_shift_held;
static bool g_right_shift_held;

void MenuInit(const MenuConfig* config) { g_config = *config; }

bool MenuIsOpen(void) { return g_open; }

const char* MenuPageName(void) {
  const MenuPageDef* def = &kPageDefs[g_page];
  return def->get_name != NULL ? def->get_name() : def->name;
}

static void ShowPage(MenuPage page, uint8_t chosen);
static MenuAction Choose(void);
static MenuAction Back(void);
static void ShowVM(uint8_t index);
static uint8_t FormFirstItem(void);
static uint8_t FormItem(void);
static const NewDriveItem* DriveChoices(uint8_t* count);
static uint8_t DriveChoiceRow(NewDriveItem choice);
static void ScrollToChosen(void);

void MenuChooseVMs(const VMList* list, uint8_t highlighted) {
  g_vms = list;
  g_open = true;
  g_choosing = true;
  g_box_drawn = false;
  g_list_top = 0;
  ShowPage(kMenuPageVMs, highlighted);
  ScrollToChosen();
}

uint8_t MenuChosenVM(void) { return g_chosen; }

Font4x10 MenuChosenFont(void) { return g_font; }

void MenuSetFont(Font4x10 font) {
  if (font != g_font) {
    g_font = font;
    g_dirty = true;
  }
}

void MenuClose(void) {
  g_open = false;
  g_choosing = false;
}

void MenuShowSharing(void) { ShowPage(kMenuPageSharing, 0); }

void MenuSetCountdown(uint8_t seconds) {
  if (seconds != g_countdown) {
    g_countdown = seconds;
    g_dirty = true;
  }
}

void MenuSetProblem(const char* problem) {
  g_problem = g_problem == g_problems[0] ? g_problems[1] : g_problems[0];
  snprintf(g_problem, kTextColumns + 1, "%s", problem != NULL ? problem : "");
}

const char* MenuHint(void) {
  const MenuPageDef* def = &kPageDefs[g_page];
  if (def->get_hint != NULL) {
    return def->get_hint();
  }
  return def->hint != NULL ? def->hint : "\x18\x19=Move  Enter=Select  Esc=Back";
}

const char* MenuProblem(void) {
  return g_problem[0] != '\0' ? g_problem : NULL;
}

// Show the browser on a folder, with the image in the drive chosen if it is
// there.
static void Browse(const char* path) {
  // A floppy drive takes a floppy, drive C a hard disk, and USB either.
  static const uint32_t kImageSizes[] = {
      kFloppyImageSize, Y_HDC_GEOMETRY_10MB_IMAGE_SIZE};
  const uint32_t* sizes = kImageSizes;
  uint8_t num_sizes = 1;
  if (g_browsing_for == kBrowseForUSB) {
    num_sizes = 2;
  } else if (g_browsing_for == kBrowseForNewVM && g_new_drive >= kVMDriveHD1) {
    sizes = &kImageSizes[1];
  }
  FileListRead(&g_list, path, sizes, num_sizes);
  g_page = kMenuPageBrowse;
  g_chosen = 0;
  g_list_top = 0;
  g_dirty = true;
  // The image to choose: the one in the drive, or chosen for the new VM's.
  // The drives have no images yet at power on.
  const char* chosen = NULL;
  if (g_browsing_for == kBrowseForNewVM) {
    chosen = g_new_drives[g_new_drive];
  } else if (g_browsing_for == kBrowseForDrive) {
    const DiskImage* image = g_config.drive(g_drive);
    if (image->status == kDiskImageOpen) {
      chosen = image->path;
    }
  }
  if (chosen == NULL || chosen[0] == '\0') {
    return;
  }
  char path_of_entry[kFSMaxPath + 1];
  for (uint8_t i = 0; i < g_list.count; ++i) {
    if (g_list.entries[i].kind == kFileListImage &&
        FileListEntryPath(&g_list, &g_list.entries[i], path_of_entry) &&
        FSSamePath(path_of_entry, chosen)) {
      g_chosen = i;
      ScrollToChosen();
    }
  }
}

// The folder the browser starts in: that of the image in the drive, or else
// of the one in the other drive - the next disk of a set is likely beside
// it - or else yax86.
static void StartBrowsing(void) {
  g_browsing_for = kBrowseForDrive;
  const DiskImage* image = g_config.drive(g_drive);
  if (image->status != kDiskImageOpen) {
    image = g_config.drive(
        g_drive == kFloppyDriveA ? kFloppyDriveB : kFloppyDriveA);
  }
  char folder[kFSMaxPath + 1];
  if (image->status == kDiskImageOpen) {
    FSFolderOf(image->path, folder);
  } else {
    FSCopyPath("yax86", folder);
  }
  Browse(folder);
}

// Keep the chosen entry among the browser's rows.
static void ScrollToChosen(void) {
  uint8_t row = g_chosen;
  if ((g_page == kMenuPageVMs || g_page == kMenuPageManage) &&
      g_vms->count > 0 && g_chosen >= g_vms->count) {
    ++row;
  }
  UIBoxScrollTo(row, &g_list_top, kBrowseRows);
}

// Show the browser on the folder above, with the folder just left chosen.
static void BrowseUp(void) {
  char left[kFSMaxPath + 1];
  char parent[kFSMaxPath + 1];
  FSCopyPath(g_list.path, left);
  FSFolderOf(left, parent);
  Browse(parent);
  char path_of_entry[kFSMaxPath + 1];
  for (uint8_t i = 0; i < g_list.count; ++i) {
    if (g_list.entries[i].kind == kFileListFolder &&
        FileListEntryPath(&g_list, &g_list.entries[i], path_of_entry) &&
        FSSamePath(path_of_entry, left)) {
      g_chosen = i;
      ScrollToChosen();
    }
  }
}

static void ShowPage(MenuPage page, uint8_t chosen) {
  g_page = page;
  g_chosen = chosen;
  g_dirty = true;
}

static bool DriveEmpty(void) {
  return g_config.drive(g_drive)->status == kDiskImageEjected;
}

// The main page's items, as they show: Floppy disk 2 only with a second
// floppy drive, and Power off only with a list to go back to. Returns how
// many.
static uint8_t MainItems(MainItem items[kMainItemCount]) {
  uint8_t count = 0;
  for (uint8_t item = 0; item < kMainItemCount; ++item) {
    if ((item == kMainDriveB && g_config.floppy_drives() < 2) ||
        (item == kMainPowerOff && !g_config.can_power_off())) {
      continue;
    }
    items[count++] = (MainItem)item;
  }
  return count;
}

static uint8_t MainItemCount(void) {
  MainItem items[kMainItemCount];
  return MainItems(items);
}

// The main page's item on the row chosen.
static MainItem ChosenMainItem(void) {
  MainItem items[kMainItemCount];
  const uint8_t count = MainItems(items);
  return g_chosen < count ? items[g_chosen] : kMainItemCount;
}

// Show the main page with an item chosen, or the first if it is not shown.
static void ShowMainItem(MainItem item) {
  MainItem items[kMainItemCount];
  const uint8_t count = MainItems(items);
  uint8_t row = 0;
  for (uint8_t i = 0; i < count; ++i) {
    if (items[i] == item) {
      row = i;
    }
  }
  ShowPage(kMenuPageMain, row);
}

static uint8_t DriveItemCount(void) {
  return DriveEmpty() ? kDriveEject : kDriveItemCount;
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

static uint8_t ManageRowCount(void) {
  return (uint8_t)(g_vms->count + (g_vms->count > 0 ? 2 : 1));
}

// Whether disconnecting would take the card from a computer still using it.
static bool ComputerHasCard(void) {
  const UsbDiskStats stats = UsbDiskGetStats();
  return stats.connected && stats.seen && !stats.ejected;
}

static uint8_t ResetItemCount(void) { return kResetItemCount; }
static uint8_t PowerOffItemCount(void) { return kPowerOffItemCount; }
static uint8_t BrowseItemCount(void) { return g_list.count; }
static uint8_t USBItemCount(void) { return kUSBItemCount; }
static uint8_t SharingItemCount(void) { return 1; }
static uint8_t StopSharingItemCount(void) { return kStopSharingItemCount; }
static uint8_t ManageItemCount(void) { return (uint8_t)(g_vms->count + 1); }
static uint8_t VMItemCount(void) { return kVMItemCount; }
static uint8_t DeleteItemCount(void) { return kDeleteItemCount; }
static uint8_t NewItemCount(void) {
  return (uint8_t)(kNewItemCount - FormFirstItem());
}
static uint8_t NewDriveItemCount(void) {
  return DriveChoiceRow(kNewDriveItemCount);
}
static uint8_t SettingsItemCount(void) { return kSettingsItemCount; }
static uint8_t FontItemCount(void) { return kFont4x10Count; }

static uint8_t PageItemCount(void) {
  const MenuPageDef* def = &kPageDefs[g_page];
  return def->item_count != NULL ? def->item_count() : 0;
}

// Show Manage VMs - a row for each VM, then New VM - with the item at index
// chosen.
static void ShowManaged(uint8_t index) {
  g_list_top = 0;
  ShowPage(kMenuPageManage, index);
  ScrollToChosen();
}

void MenuShowManagedVMs(const char* name, bool show_vm) {
  const uint8_t index = VMListFind(g_vms, name);
  ShowManaged(index < g_vms->count ? index : 0);
  if (show_vm && index < g_vms->count) {
    ShowVM(index);
  }
}

// Show a VM's page, with the images in its drives.
static void ShowVM(uint8_t index) {
  g_vm = index;
  VMConfig config;
  char problem[kVMProblemLength + 1];
  g_vm_readable = VMConfigRead(&g_vms->vms[index], &config, problem);
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    g_vm_drives[drive][0] = '\0';
    if (config.drives[drive][0] != '\0') {
      DiskImageName(config.drives[drive], g_vm_drives[drive]);
    }
    g_vm_present[drive] =
        drive == kVMDriveFD1 ||
        (drive == kVMDriveFD2 ? config.present[drive]
                              : config.drives[drive][0] != '\0');
  }
  ShowPage(kMenuPageVM, kVMEdit);
}

// Start typing a name: the VM's own to rename it, and for a clone the same
// with _2 at the end, in the name's 8 characters.
static void StartNaming(bool cloning) {
  g_cloning = cloning;
  const char* name = g_vms->vms[g_vm].name;
  g_name_length = 0;
  while (name[g_name_length] != '\0' && g_name_length < kFSMaxFolderName) {
    g_name[g_name_length] = name[g_name_length];
    ++g_name_length;
  }
  if (cloning) {
    if (g_name_length > kFSMaxFolderName - 2) {
      g_name_length = kFSMaxFolderName - 2;
    }
    g_name[g_name_length++] = '_';
    g_name[g_name_length++] = '2';
  }
  g_name[g_name_length] = '\0';
  ShowPage(kMenuPageName, 0);
}

// A key on the name's page: a character a name can have to add, Backspace to
// take the last off, Enter to go ahead, and Esc to go back.
static MenuAction TypeName(uint8_t key) {
  switch (key) {
    case kKeyEnter:
      return Choose();
    case kKeyEsc:
      return Back();
    case kKeyBackspace:
      if (g_name_length > 0) {
        g_name[--g_name_length] = '\0';
        g_dirty = true;
      }
      return kMenuActionNone;
    default:
      break;
  }
  if (key < 0x80 && FSIsFolderNameCharacter((char)key) &&
      g_name_length < kFSMaxFolderName) {
    g_name[g_name_length++] = (char)toupper(key);
    g_name[g_name_length] = '\0';
    g_dirty = true;
  }
  return kMenuActionNone;
}

uint8_t MenuManagedVM(void) { return g_vm; }

const char* MenuTypedName(void) { return g_name; }

void MenuSetCopyProgress(uint32_t copied_kb, uint32_t total_kb) {
  if (g_page != kMenuPageCopying) {
    g_creating = g_page == kMenuPageNew;
    ShowPage(kMenuPageCopying, 0);
  }
  g_copied_kb = copied_kb;
  g_copy_kb = total_kb;
  g_dirty = true;
}

// The form's first item: its name for a new VM; editing one, it starts at
// the drives. g_chosen counts from it.
static uint8_t FormFirstItem(void) { return g_editing ? kNewDriveA : kNewName; }

static uint8_t FormItem(void) { return (uint8_t)(g_chosen + FormFirstItem()); }

static void ShowFormItem(uint8_t item) {
  ShowPage(kMenuPageNew, (uint8_t)(item - FormFirstItem()));
}

// Start a new VM's form: no name, and every drive empty.
static void StartNewVM(void) {
  g_editing = false;
  g_name[0] = '\0';
  g_name_length = 0;
  // Floppy disk 1 empty, floppy disk 2 and hard disk 2 disabled, and a new
  // disk image as hard disk 1.
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    g_new_drives[drive][0] = '\0';
    g_new_blank[drive] = drive == kVMDriveHD1;
    g_new_present[drive] = drive == kVMDriveFD1 || drive == kVMDriveHD1;
  }
  ShowFormItem(kNewName);
}

// Start the form on the drives of the VM whose page shows, as its vm.cfg has
// them.
static void StartEditVM(void) {
  g_editing = true;
  static VMConfig config;
  char problem[kVMProblemLength + 1];
  VMConfigRead(&g_vms->vms[g_vm], &config, problem);
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    FSCopyPath(config.drives[drive], g_new_drives[drive]);
    g_new_blank[drive] = false;
    g_new_present[drive] =
        drive == kVMDriveFD1 ||
        (drive == kVMDriveFD2 ? config.present[drive]
                              : config.drives[drive][0] != '\0');
  }
  ShowFormItem(kNewDriveA);
}

// The choices the drive on show offers, and how many.
static const NewDriveItem* DriveChoices(uint8_t* count) {
  if (g_new_drive == kVMDriveFD1) {
    *count = sizeof(kFloppy1Choices) / sizeof(kFloppy1Choices[0]);
    return kFloppy1Choices;
  }
  if (g_new_drive == kVMDriveFD2) {
    *count = sizeof(kFloppy2Choices) / sizeof(kFloppy2Choices[0]);
    return kFloppy2Choices;
  }
  *count = sizeof(kHardDiskChoices) / sizeof(kHardDiskChoices[0]);
  return kHardDiskChoices;
}

// The row of a choice on the drive's page, or with kNewDriveItemCount, how
// many rows there are.
static uint8_t DriveChoiceRow(NewDriveItem choice) {
  uint8_t count = 0;
  const NewDriveItem* choices = DriveChoices(&count);
  for (uint8_t row = 0; row < count; ++row) {
    if (choices[row] == choice) {
      return row;
    }
  }
  return choice == kNewDriveItemCount ? count : 0;
}

// Show the choices for a drive on the form, with what it has chosen.
static void ShowNewDrive(uint8_t drive) {
  g_new_drive = drive;
  uint8_t chosen = kNewDriveNone;
  if (!g_new_present[drive]) {
    chosen = kNewDriveDisabled;
  } else if (g_new_blank[drive]) {
    chosen = kNewDriveBlank;
  } else if (g_new_drives[drive][0] != '\0') {
    chosen = kNewDriveImage;
  }
  ShowPage(kMenuPageNewDrive, DriveChoiceRow((NewDriveItem)chosen));
}

// Take a choice for a drive on the form, back to the form; or for an image,
// find it in the browser, starting where the one chosen is, if one is.
static void ChooseNewDrive(void) {
  uint8_t count = 0;
  const NewDriveItem choice = DriveChoices(&count)[g_chosen];
  char* path = g_new_drives[g_new_drive];
  if (choice == kNewDriveImage) {
    g_browsing_for = kBrowseForNewVM;
    char folder[kFSMaxPath + 1];
    if (path[0] != '\0') {
      FSFolderOf(path, folder);
    } else {
      FSCopyPath("yax86", folder);
    }
    Browse(folder);
    return;
  }
  path[0] = '\0';
  g_new_blank[g_new_drive] = choice == kNewDriveBlank;
  g_new_present[g_new_drive] = choice != kNewDriveDisabled;
  ShowFormItem((uint8_t)(kNewDriveA + g_new_drive));
}

// A key on the new VM's form while its name is chosen: a character a name
// can have to add, and Backspace to take the last off. Returns whether the
// key was the name's.
static bool TypeNewName(uint8_t key) {
  if (key == kKeyBackspace) {
    if (g_name_length > 0) {
      g_name[--g_name_length] = '\0';
      g_dirty = true;
    }
    return true;
  }
  if (key >= 0x80 || !FSIsFolderNameCharacter((char)key)) {
    return false;
  }
  if (g_name_length < kFSMaxFolderName) {
    g_name[g_name_length++] = (char)toupper(key);
    g_name[g_name_length] = '\0';
    g_dirty = true;
  }
  return true;
}

void MenuFormDrives(VMFolderDrives* drives) {
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    drives->paths[drive] = g_new_present[drive] ? g_new_drives[drive] : NULL;
    drives->blank[drive] = g_new_blank[drive];
  }
}

// Disconnect the card: at once if no computer is using it, and after asking if
// one is.
static MenuAction Disconnect(void) {
  if (ComputerHasCard()) {
    ShowPage(kMenuPageStopSharing, kStopSharingCancel);
    return kMenuActionNone;
  }
  return kMenuActionStopSharing;
}

static const char* DrivePageName(void) {
  return g_drive == kFloppyDriveA ? "fd1" : "fd2";
}

static const char* NamePageHint(void) {
  return g_cloning ? "Enter=Clone  Backspace=Erase  Esc=Cancel"
                   : "Enter=Rename  Backspace=Erase  Esc=Cancel";
}

static const char* NewPageHint(void) {
  return FormItem() == kNewName
             ? "Type the name  \x18\x19=Move  Backspace=Erase  Esc=Cancel"
             : "\x18\x19=Move  Enter=Select  Esc=Cancel";
}

static bool HandleKeyName(SouthbridgeKeyEvent event, MenuAction* action) {
  *action = TypeName(event.key);
  return true;
}

static bool HandleKeyCopying(SouthbridgeKeyEvent event, MenuAction* action) {
  (void)event;
  *action = kMenuActionNone;
  return true;
}

static bool HandleKeyNew(SouthbridgeKeyEvent event, MenuAction* action) {
  if ((FormItem() == kNewName && TypeNewName(event.key)) ||
      event.key == kKeyBackspace) {
    *action = kMenuActionNone;
    return true;
  }
  return false;
}

static MenuAction BackMain(void) {
  g_open = false;
  return kMenuActionClosed;
}

static MenuAction BackStatus(void) {
  ShowMainItem(kMainStatus);
  return kMenuActionNone;
}

static MenuAction BackReset(void) {
  ShowMainItem(kMainReset);
  return kMenuActionNone;
}

static MenuAction BackPowerOff(void) {
  ShowMainItem(kMainPowerOff);
  return kMenuActionNone;
}

static MenuAction BackDrive(void) {
  ShowMainItem((MainItem)(kMainDriveA + g_drive));
  return kMenuActionNone;
}

static MenuAction BackBrowse(void) {
  if (g_browsing_for == kBrowseForUSB) {
    ShowPage(kMenuPageUSB, kUSBImage);
  } else if (g_browsing_for == kBrowseForNewVM) {
    ShowPage(kMenuPageNewDrive, DriveChoiceRow(kNewDriveImage));
  } else {
    ShowPage(kMenuPageDrive, kDriveInsert);
  }
  return kMenuActionNone;
}

static MenuAction BackUSB(void) {
  ShowPage(kMenuPageVMs, USBItemIndex());
  ScrollToChosen();
  return kMenuActionNone;
}

static MenuAction BackStopSharing(void) {
  ShowPage(kMenuPageSharing, 0);
  return kMenuActionNone;
}

static MenuAction BackManage(void) {
  ShowPage(kMenuPageVMs, ManageItemIndex());
  ScrollToChosen();
  return kMenuActionNone;
}

static MenuAction BackVM(void) {
  ShowManaged(g_vm);
  return kMenuActionNone;
}

static MenuAction BackName(void) {
  ShowPage(kMenuPageVM, g_cloning ? kVMClone : kVMRename);
  return kMenuActionNone;
}

static MenuAction BackDelete(void) {
  ShowPage(kMenuPageVM, kVMDelete);
  return kMenuActionNone;
}

static MenuAction BackNew(void) {
  if (g_editing) {
    ShowPage(kMenuPageVM, kVMEdit);
  } else {
    ShowManaged(g_vms->count);
  }
  return kMenuActionNone;
}

static MenuAction BackNewDrive(void) {
  ShowFormItem((uint8_t)(kNewDriveA + g_new_drive));
  return kMenuActionNone;
}

static MenuAction BackSettings(void) {
  ShowPage(kMenuPageVMs, SettingsItemIndex());
  ScrollToChosen();
  return kMenuActionNone;
}

static MenuAction BackFont(void) {
  ShowPage(kMenuPageSettings, 0);
  return kMenuActionNone;
}

static MenuAction Back(void) {
  const MenuPageDef* def = &kPageDefs[g_page];
  return def->back != NULL ? def->back() : kMenuActionNone;
}

// Open the chosen entry of the browser: a folder to look in, or an image to
// put in the drive, which closes the menu.
static MenuAction OpenEntry(void) {
  if (g_chosen >= g_list.count) {
    return kMenuActionNone;
  }
  const FileListEntry* entry = &g_list.entries[g_chosen];
  char path[kFSMaxPath + 1];
  if (!FileListEntryPath(&g_list, entry, path)) {
    return kMenuActionNone;
  }
  if (entry->kind == kFileListParent) {
    BrowseUp();
    return kMenuActionNone;
  }
  if (entry->kind == kFileListFolder) {
    Browse(path);
    return kMenuActionNone;
  }
  if (g_browsing_for == kBrowseForNewVM) {
    FSCopyPath(path, g_new_drives[g_new_drive]);
    g_new_blank[g_new_drive] = false;
    g_new_present[g_new_drive] = true;
    ShowFormItem((uint8_t)(kNewDriveA + g_new_drive));
    return kMenuActionNone;
  }
  if (g_browsing_for == kBrowseForUSB) {
    FSCopyPath(path, g_usb_image_path);
    g_usb_image_size = entry->size;
    return kMenuActionShareImage;
  }
  g_config.insert_drive(g_drive, path);
  g_open = false;
  return kMenuActionClosed;
}

const char* MenuChosenImage(uint32_t* size) {
  *size = g_usb_image_size;
  return g_usb_image_path;
}

static MenuAction ChooseMain(void) {
  switch (ChosenMainItem()) {
    case kMainDriveA:
    case kMainDriveB:
      g_drive = (FloppyDrive)(ChosenMainItem() - kMainDriveA);
      ShowPage(kMenuPageDrive, kDriveInsert);
      return kMenuActionNone;
    case kMainStatus:
      ShowPage(kMenuPageStatus, 0);
      return kMenuActionNone;
    case kMainReset:
      ShowPage(kMenuPageReset, kResetCancel);
      return kMenuActionNone;
    case kMainPowerOff:
      ShowPage(kMenuPagePowerOff, kPowerOffCancel);
      return kMenuActionNone;
    case kMainItemCount:
      break;
  }
  return kMenuActionNone;
}

static MenuAction ChooseReset(void) {
  if (g_chosen == kResetConfirm) {
    g_config.reset();
  }
  return BackReset();
}

static MenuAction ChoosePowerOff(void) {
  if (g_chosen == kPowerOffConfirm) {
    g_config.power_off();
  }
  return BackPowerOff();
}

static MenuAction ChooseDrive(void) {
  if (g_chosen == kDriveInsert) {
    StartBrowsing();
  } else {
    g_config.eject_drive(g_drive);
    ShowPage(kMenuPageDrive, kDriveInsert);
  }
  return kMenuActionNone;
}

static MenuAction ChooseVMs(void) {
  if (g_chosen == SettingsItemIndex()) {
    ShowPage(kMenuPageSettings, 0);
    return kMenuActionNone;
  }
  if (g_chosen == USBItemIndex()) {
    ShowPage(kMenuPageUSB, kUSBCard);
    return kMenuActionNone;
  }
  if (g_chosen == ManageItemIndex()) {
    ShowManaged(0);
    return kMenuActionNone;
  }
  return kMenuActionVMChosen;
}

static MenuAction ChooseSettings(void) {
  ShowPage(kMenuPageFont, g_font);
  return kMenuActionNone;
}

static MenuAction ChooseFont(void) {
  g_font = (Font4x10)g_chosen;
  ShowPage(kMenuPageSettings, 0);
  return kMenuActionFontChosen;
}

static MenuAction ChooseManage(void) {
  if (g_chosen == g_vms->count) {
    StartNewVM();
  } else {
    ShowVM(g_chosen);
  }
  return kMenuActionNone;
}

static MenuAction ChooseNew(void) {
  if (FormItem() == kNewCreate) {
    return g_editing ? kMenuActionEditVM : kMenuActionCreateVM;
  }
  if (FormItem() == kNewName) {
    ShowFormItem(kNewDriveA);
  } else {
    ShowNewDrive((uint8_t)(FormItem() - kNewDriveA));
  }
  return kMenuActionNone;
}

static MenuAction ChooseNewDriveOption(void) {
  ChooseNewDrive();
  return kMenuActionNone;
}

static MenuAction ChooseVM(void) {
  if (g_chosen == kVMEdit) {
    StartEditVM();
  } else if (g_chosen == kVMDelete) {
    VMFolderDescribe(g_vms, &g_vms->vms[g_vm], &g_contents);
    ShowPage(kMenuPageDelete, kDeleteCancel);
  } else {
    StartNaming(g_chosen == kVMClone);
  }
  return kMenuActionNone;
}

static MenuAction ChooseName(void) {
  return g_cloning ? kMenuActionCloneVM : kMenuActionRenameVM;
}

static MenuAction ChooseDelete(void) {
  if (g_chosen == kDeleteConfirm) {
    return kMenuActionDeleteVM;
  }
  return BackDelete();
}

static MenuAction ChooseUSB(void) {
  if (g_chosen == kUSBImage) {
    g_browsing_for = kBrowseForUSB;
    Browse("yax86");
    return kMenuActionNone;
  }
  return kMenuActionShareCard;
}

static MenuAction ChooseStopSharing(void) {
  if (g_chosen == kStopSharingConfirm) {
    return kMenuActionStopSharing;
  }
  return BackStopSharing();
}

static MenuAction Choose(void) {
  const MenuPageDef* def = &kPageDefs[g_page];
  return def->choose != NULL ? def->choose() : kMenuActionNone;
}

// Track the Shift keys, and return whether this event completes the hotkey:
// both held, the second just pressed.
static bool TrackHotkey(SouthbridgeKeyEvent event) {
  bool* held = NULL;
  if (event.key == kKeyLeftShift) {
    held = &g_left_shift_held;
  } else if (event.key == kKeyRightShift) {
    held = &g_right_shift_held;
  } else {
    return false;
  }
  const bool was_held = *held;
  *held = event.state != kSouthbridgeKeyReleased;
  return *held && !was_held && g_left_shift_held && g_right_shift_held;
}

MenuAction MenuHandleKey(SouthbridgeKeyEvent event) {
  const bool hotkey = TrackHotkey(event);
  if (g_open && g_choosing) {
    // Nothing closes the list but starting something, and Esc goes back no
    // further than it.
    if (hotkey || (g_page == kMenuPageVMs &&
                   (event.key == kKeyEsc || event.key == kKeyBackspace))) {
      return kMenuActionNone;
    }
    if (event.state != kSouthbridgeKeyReleased && g_problem[0] != '\0') {
      g_problem[0] = '\0';
      g_dirty = true;
    }
  } else if (hotkey) {
    g_open = !g_open;
    if (g_open) {
      g_box_drawn = false;
      ShowMainItem(kMainDriveA);
      return kMenuActionOpened;
    }
    return kMenuActionClosed;
  }
  // A key held down repeats, which moves through the items as a press does.
  if (!g_open || event.state == kSouthbridgeKeyReleased) {
    return kMenuActionNone;
  }
  const MenuPageDef* def = &kPageDefs[g_page];
  if (def->handle_key != NULL) {
    MenuAction action = kMenuActionNone;
    if (def->handle_key(event, &action)) {
      return action;
    }
  }
  const uint8_t count = PageItemCount();
  switch (event.key) {
    case kKeyUp:
      if (count > 0) {
        g_chosen = (uint8_t)((g_chosen + count - 1) % count);
        g_dirty = true;
      }
      ScrollToChosen();
      return kMenuActionNone;
    case kKeyDown:
      if (count > 0) {
        g_chosen = (uint8_t)((g_chosen + 1) % count);
        g_dirty = true;
      }
      ScrollToChosen();
      return kMenuActionNone;
    case kKeyPageUp:
      if (count > 0) {
        g_chosen =
            g_chosen > kBrowseRows ? (uint8_t)(g_chosen - kBrowseRows) : 0;
        g_dirty = true;
      }
      ScrollToChosen();
      return kMenuActionNone;
    case kKeyPageDown:
      if (count > 0) {
        g_chosen = g_chosen + kBrowseRows < count
                       ? (uint8_t)(g_chosen + kBrowseRows)
                       : (uint8_t)(count - 1);
        g_dirty = true;
      }
      ScrollToChosen();
      return kMenuActionNone;
    case kKeyEnter:
    case kKeySpace:
      return Choose();
    case kKeyBackspace:
      // Up a folder in the browser, until the card's root.
      if (g_page == kMenuPageBrowse && g_list.path[0] != '\0') {
        BrowseUp();
        return kMenuActionNone;
      }
      return Back();
    case kKeyEsc:
      return Back();
    default:
      return kMenuActionNone;
  }
}

// ============================================================================
// Drawing
// ============================================================================

// A page of items, under lines of text, if any.
static void DrawItems(
    DisplayMenu* menu, const char* title, const char* const* lines,
    uint8_t line_count, const MenuItem* items, uint8_t count) {
  UIBoxDrawItems(
      menu, title, lines, line_count, items, count, g_chosen, g_choosing);
}

static void DrawStatusPage(DisplayMenu* menu) {
  MenuStatusLine lines[kMenuStatusLines];
  g_config.describe_status(lines);
  UIBoxPlace(menu, kUIBoxWideColumns, kMenuStatusLines + 4);
  uint8_t row = 0;
  UIBoxEdgeRow(&menu->text[row++], menu->columns, true, "Status");
  UIBoxBlankRow(&menu->text[row++], menu->columns);
  for (uint8_t i = 0; i < kMenuStatusLines; ++i) {
    DisplayTextRow* text = &menu->text[row++];
    UIBoxBlankRow(text, menu->columns);
    UIBoxPutText(text, menu->columns, 2, lines[i].label, kUIBoxAttrTitle);
    UIBoxPutText(
        text, menu->columns, 2 + kStatusLabelColumns, lines[i].value,
        kUIBoxAttrText);
  }
  UIBoxBlankRow(&menu->text[row++], menu->columns);
  UIBoxEdgeRow(&menu->text[row], menu->columns, false, NULL);
}

static void DrawDrivePage(DisplayMenu* menu) {
  const DiskImage* image = g_config.drive(g_drive);
  char line[kUIBoxNarrowColumns];
  const char* in_drive = "the built-in boot floppy";
  char name[kDiskImageNameLength + 1];
  const bool empty = DriveEmpty();
  if (image->status == kDiskImageOpen) {
    DiskImageName(image->path, name);
    in_drive = name;
  } else if (empty) {
    in_drive = "empty";
  }
  const MenuItem items[kDriveItemCount] = {
      [kDriveInsert] = {empty ? "Choose disk image" : "Swap disk image", true},
      [kDriveEject] = {"Eject", false},
  };
  snprintf(line, sizeof(line), "In the drive: %s", in_drive);
  const char* const lines[] = {
      line,
      "Change disks only while DOS is",
      "not using the drive.",
  };
  const char* title = kVMDriveNames[kVMDriveFD1 + g_drive];
  DrawItems(
      menu, title, lines, sizeof(lines) / sizeof(lines[0]), items,
      DriveItemCount());
}

// What the browser says of a folder with nothing to choose in it.
static const char* BrowseEmpty(void) {
  if (g_browsing_for == kBrowseForUSB) {
    return "No folders or disk images";
  }
  if (g_browsing_for == kBrowseForNewVM && g_new_drive >= kVMDriveHD1) {
    return "No folders or hard disk images";
  }
  return "No folders or floppy images";
}

// The browser: the folder as the title, and a row for each entry that
// shows, with arrows on the border where there is more above or below.
static void DrawBrowsePage(DisplayMenu* menu) {
  UIBoxPlace(menu, kUIBoxNarrowColumns, kBrowseRows + 2);
  // The folder, from the card's root, keeping its end if it is too long.
  char title[kUIBoxNarrowColumns - 6 + 1];
  const uint8_t room = sizeof(title) - 1;
  char full[kFSMaxPath + 2];
  snprintf(full, sizeof(full), "/%s", g_list.path);
  for (char* c = full; *c != '\0'; ++c) {
    *c = (char)toupper((unsigned char)*c);
  }
  const size_t length = strlen(full);
  if (length > room) {
    snprintf(title, sizeof(title), "..%s", &full[length - room + 2]);
  } else {
    snprintf(title, sizeof(title), "%s", full);
  }
  UIBoxEdgeRow(&menu->text[0], menu->columns, true, title);
  for (uint8_t i = 0; i < kBrowseRows; ++i) {
    DisplayTextRow* row = &menu->text[1 + i];
    const uint8_t index = (uint8_t)(g_list_top + i);
    if (index >= g_list.count) {
      UIBoxBlankRow(row, menu->columns);
      if (i == 0 && !g_list.readable) {
        UIBoxPutText(
            row, menu->columns, 3, "The card cannot be read", kUIBoxAttrText);
      } else if (i == 1 && g_list.count <= 1) {
        UIBoxPutText(row, menu->columns, 3, BrowseEmpty(), kUIBoxAttrText);
      }
      continue;
    }
    const FileListEntry* entry = &g_list.entries[index];
    UIBoxItemRow(row, menu->columns, entry->name, index == g_chosen);
    const uint8_t attr = index == g_chosen ? kUIBoxAttrChosen : kUIBoxAttrText;
    char path[kFSMaxPath + 1];
    if (entry->kind != kFileListImage) {
      UIBoxPutText(row, menu->columns, kBrowseNoteColumn, "<DIR>", attr);
      continue;
    }
    if (g_browsing_for == kBrowseForUSB) {
      UIBoxPutText(
          row, menu->columns, kBrowseNoteColumn,
          entry->size == kFloppyImageSize ? "floppy" : "hard disk", attr);
      continue;
    }
    if (g_browsing_for == kBrowseForNewVM) {
      continue;
    }
    if (!FileListEntryPath(&g_list, entry, path)) {
      continue;
    }
    for (uint8_t drive = 0; drive < kFloppyDrives; ++drive) {
      const DiskImage* image = g_config.drive((FloppyDrive)drive);
      if (image->status == kDiskImageOpen && FSSamePath(path, image->path)) {
        char note[16];
        snprintf(note, sizeof(note), "in %s", kVMDriveLabels[drive]);
        UIBoxPutText(row, menu->columns, kBrowseNoteColumn, note, attr);
      }
    }
  }
  // A folder with more entries than a list holds says so on the bottom edge.
  UIBoxEdgeRow(
      &menu->text[kBrowseRows + 1], menu->columns, false,
      g_list.truncated ? "the first 64 entries" : NULL);
  UIBoxDrawScrollArrows(menu, g_list_top, kBrowseRows, g_list.count);
}

// The VMs to start: a row each, with a line under them if there are any,
// arrows on the border where there are more above or below, and the
// countdown on the bottom edge.
static void DrawVMsPage(DisplayMenu* menu) {
  UIBoxPlace(menu, kUIBoxNarrowColumns, kBrowseRows + 2);
  UIBoxEdgeRow(&menu->text[0], menu->columns, true, "YAX86");
  for (uint8_t i = 0; i < kBrowseRows; ++i) {
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
  char countdown[kUIBoxNarrowColumns];
  if (g_countdown > 0) {
    snprintf(
        countdown, sizeof(countdown), "starting in %u s",
        (unsigned)g_countdown);
  }
  UIBoxEdgeRow(
      &menu->text[kBrowseRows + 1], menu->columns, false,
      g_countdown > 0 ? countdown : NULL);
  UIBoxDrawScrollArrows(menu, g_list_top, kBrowseRows, VMsRowCount());
}

// Manage VMs: the VMs in yax86/vm, a row each, then New VM after a line if
// there are any, with arrows on the border where there are more above or
// below.
static void DrawManagePage(DisplayMenu* menu) {
  UIBoxPlace(menu, kUIBoxNarrowColumns, kBrowseRows + 2);
  UIBoxEdgeRow(&menu->text[0], menu->columns, true, "Manage VMs");
  for (uint8_t i = 0; i < kBrowseRows; ++i) {
    DisplayTextRow* row = &menu->text[1 + i];
    const uint8_t index = (uint8_t)(g_list_top + i);
    if (index >= ManageRowCount()) {
      UIBoxBlankRow(row, menu->columns);
    } else if (index < g_vms->count) {
      const MenuItem item = {g_vms->vms[index].name, true};
      UIBoxMenuItemRow(row, menu->columns, &item, index == g_chosen);
    } else if (g_vms->count > 0 && index == g_vms->count) {
      UIBoxSeparatorRow(row, menu->columns);
    } else {
      UIBoxMenuItemRow(
          row, menu->columns, &kNewVMRow, g_chosen == g_vms->count);
    }
  }
  UIBoxEdgeRow(&menu->text[kBrowseRows + 1], menu->columns, false, NULL);
  UIBoxDrawScrollArrows(menu, g_list_top, kBrowseRows, ManageRowCount());
}

// A VM's page: the images in its drives, over what can be done to it.
// What a VM's drive holds, for its page: an image's name, "empty" for a
// floppy drive with none, or "disabled" for a drive not there.
static const char* DriveShown(uint8_t drive, const char* name) {
  if (name[0] != '\0') {
    return name;
  }
  return g_vm_present[drive] ? "empty" : "disabled";
}

static void DrawVMPage(DisplayMenu* menu) {
  // A line for each pair of drives - FD1 and FD2, HD1 and HD2 - that holds
  // anything,
  // with room for any name, which the box cuts to its width.
  char lines[kVMDrives / 2][64];
  const char* line_pointers[kVMDrives / 2];
  uint8_t line_count = 0;
  for (uint8_t first = 0; first < kVMDrives; first += 2) {
    char label_a[8];
    char label_b[8];
    snprintf(label_a, sizeof(label_a), "%s:", kVMDriveLabels[first]);
    snprintf(label_b, sizeof(label_b), "%s:", kVMDriveLabels[first + 1]);
    const char* a = g_vm_drives[first];
    const char* b = g_vm_drives[first + 1];
    if (a[0] == '\0' && b[0] == '\0') {
      continue;
    }
    snprintf(
        lines[line_count], sizeof(lines[line_count]),
        "%-4s %-12.12s %-4s %.12s", label_a, DriveShown(first, a), label_b,
        DriveShown((uint8_t)(first + 1), b));
    line_pointers[line_count] = lines[line_count];
    ++line_count;
  }
  if (!g_vm_readable) {
    line_pointers[0] = "Its vm.cfg has a problem.";
    line_count = 1;
  } else if (line_count == 0) {
    line_pointers[0] = "No disks.";
    line_count = 1;
  }
  DrawItems(
      menu, g_vms->vms[g_vm].name, line_pointers, line_count, kVMItems,
      kVMItemCount);
}

// The name being typed, in a field marked as the item chosen is, with a
// cursor after it while there is room.
static void DrawNamePage(DisplayMenu* menu) {
  char title[kUIBoxNarrowColumns];
  snprintf(
      title, sizeof(title), "%s %s", g_cloning ? "Clone" : "Rename",
      g_vms->vms[g_vm].name);
  const char* const lines[] = {
      g_cloning ? "Name of the copy:" : "New name:",
      "",
      "",
      "Up to 8 letters, digits, - and _.",
  };
  DrawItems(menu, title, lines, sizeof(lines) / sizeof(lines[0]), NULL, 0);
  // The field, on the blank line: the box's edge, a blank row, then the
  // first line.
  DisplayTextRow* row = &menu->text[3];
  char field[kFSMaxFolderName + 1];
  for (uint8_t i = 0; i < kFSMaxFolderName; ++i) {
    field[i] = i < g_name_length ? g_name[i] : ' ';
  }
  if (g_name_length < kFSMaxFolderName) {
    field[g_name_length] = '_';
  }
  field[kFSMaxFolderName] = '\0';
  UIBoxPutText(row, menu->columns, 3, field, kUIBoxAttrChosen);
}

// Join names into text as "A, B and C", or with fewer shown, "A, B and 2
// more": as many of the first named as fit with the rest counted after.
static void JoinNames(
    char* text, uint8_t size, char (*names)[kFSMaxName + 1], uint8_t named,
    uint8_t count) {
  for (uint8_t shown = named;; --shown) {
    size_t length = 0;
    text[0] = '\0';
    for (uint8_t i = 0; i < shown && length < size; ++i) {
      const char* separator = i == 0 ? "" : (i + 1 == count ? " and " : ", ");
      length += (size_t)snprintf(
          text + length, size - length, "%s%s", separator, names[i]);
    }
    if (shown < count && length < size) {
      length += (size_t)snprintf(
          text + length, size - length, "%s%u more", shown > 0 ? " and " : "",
          (unsigned)(count - shown));
    }
    if (length < size || shown == 0) {
      return;
    }
  }
}

// The question before deleting a VM, saying what goes with it, and which VMs
// lose a disk.
static void DrawDeletePage(DisplayMenu* menu) {
  const uint8_t named = g_contents.file_count < kVMFolderNamed
                            ? g_contents.file_count
                            : kVMFolderNamed;
  char question[kUIBoxNarrowColumns];
  snprintf(question, sizeof(question), "Delete %s?", g_vms->vms[g_vm].name);
  char files[kUIBoxNarrowColumns - 4];
  JoinNames(
      files, sizeof(files), g_contents.files, named, g_contents.file_count);
  char users[64];
  const char* lines[5] = {question, ""};
  uint8_t line_count = 1;
  if (g_contents.file_count > 0 || g_contents.user_count > 0) {
    ++line_count;
  }
  if (g_contents.file_count > 0) {
    lines[line_count++] = "Deleted with it:";
    lines[line_count++] = files;
  }
  if (g_contents.user_count == 1) {
    snprintf(users, sizeof(users), "%s will lose a disk.", g_contents.users[0]);
    lines[line_count++] = users;
  } else if (g_contents.user_count > 1) {
    snprintf(
        users, sizeof(users), "%s and %u more VMs will lose disks.",
        g_contents.users[0], (unsigned)(g_contents.user_count - 1));
    lines[line_count++] = users;
  }
  DrawItems(menu, "Delete", lines, line_count, kDeleteItems, kDeleteItemCount);
}

// A clone being copied, or a new VM's blank disks being written.
static void DrawCopyingPage(DisplayMenu* menu) {
  char copying[kUIBoxNarrowColumns];
  if (g_creating) {
    snprintf(
        copying, sizeof(copying), "Writing %s's blank disks...",
        g_editing ? g_vms->vms[g_vm].name : g_name);
  } else {
    snprintf(
        copying, sizeof(copying), "Copying %s to %s...", g_vms->vms[g_vm].name,
        g_name);
  }
  char progress[kUIBoxNarrowColumns];
  snprintf(
      progress, sizeof(progress), "%lu of %lu KB", (unsigned long)g_copied_kb,
      (unsigned long)g_copy_kb);
  const char* const lines[] = {copying, progress};
  DrawItems(
      menu, g_creating ? (g_editing ? "Edit drives" : "New VM") : "Clone",
      lines, 2, NULL, 0);
}

// The form: a new VM's name, typed in place with a cursor while it is
// chosen, and the drives, with what each holds - or editing a VM's drives,
// the drives alone, under its name.
static void DrawNewPage(DisplayMenu* menu) {
  char rows[kNewItemCount][kUIBoxNarrowColumns];
  MenuItem items[kNewItemCount];
  const char* cursor =
      FormItem() == kNewName && g_name_length < kFSMaxFolderName ? "_" : "";
  snprintf(
      rows[kNewName], sizeof(rows[kNewName]), "%-15s %s%s", "Name", g_name,
      cursor);
  items[kNewName] = (MenuItem){rows[kNewName], false};
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    const uint8_t item = (uint8_t)(kNewDriveA + drive);
    const char* what = "empty";
    char name[kDiskImageNameLength + 1];
    if (!g_new_present[drive]) {
      what = "disabled";
    } else if (g_new_blank[drive]) {
      what = drive >= kVMDriveHD1 ? "new disk image" : "new floppy image";
    } else if (g_new_drives[drive][0] != '\0') {
      DiskImageName(g_new_drives[drive], name);
      what = name;
    }
    snprintf(
        rows[item], sizeof(rows[item]), "%-15s %s", kVMDriveNames[drive], what);
    items[item] = (MenuItem){rows[item], true};
  }
  items[kNewCreate] = (MenuItem){g_editing ? "Save" : "Create", false};
  DrawItems(
      menu, g_editing ? g_vms->vms[g_vm].name : "New VM", NULL, 0,
      &items[FormFirstItem()], (uint8_t)(kNewItemCount - FormFirstItem()));
}

// The main page: its items as they show.
static void DrawMainPage(DisplayMenu* menu) {
  MainItem shown[kMainItemCount];
  const uint8_t count = MainItems(shown);
  MenuItem items[kMainItemCount];
  for (uint8_t i = 0; i < count; ++i) {
    items[i] = kMainItems[shown[i]];
  }
  DrawItems(menu, "yax86", NULL, 0, items, count);
}

// The choices for one of the form's drives.
static void DrawNewDrivePage(DisplayMenu* menu) {
  uint8_t count = 0;
  const NewDriveItem* choices = DriveChoices(&count);
  const MenuItem* labels =
      g_new_drive >= kVMDriveHD1 ? kNewHardDiskItems : kNewFloppyItems;
  MenuItem items[kNewDriveItemCount];
  for (uint8_t row = 0; row < count; ++row) {
    items[row] = labels[choices[row]];
  }
  DrawItems(menu, kVMDriveNames[g_new_drive], NULL, 0, items, count);
}

static void DrawFontPage(DisplayMenu* menu) {
  MenuItem items[kFont4x10Count];
  for (uint8_t i = 0; i < kFont4x10Count; ++i) {
    items[i] = (MenuItem){Font4x10Label((Font4x10)i), false};
  }
  DrawItems(menu, "Font", NULL, 0, items, kFont4x10Count);
}

// The card while it is on USB: who has it, and how much has moved.
static void DrawSharingPage(DisplayMenu* menu) {
  const UsbDiskStats stats = UsbDiskGetStats();
  const char* state = "Waiting for the computer...";
  if (!stats.connected) {
    state = "Connect a computer by USB.";
  } else if (stats.ejected) {
    state = "Ejected by the computer.";
  } else if (stats.seen) {
    state = "In use by the computer.";
  }
  // Room for two of the largest counts, which a box cuts to its width.
  char moved[64];
  snprintf(
      moved, sizeof(moved), "Read %lu KB, written %lu KB",
      (unsigned long)(stats.blocks_read / 2),
      (unsigned long)(stats.blocks_written / 2));
  char what[kUIBoxNarrowColumns];
  if (stats.image_path != NULL) {
    char name[kDiskImageNameLength + 1];
    DiskImageName(stats.image_path, name);
    snprintf(what, sizeof(what), "Sharing %s over USB.", name);
  } else {
    snprintf(what, sizeof(what), "Sharing the SD card over USB.");
  }
  const char* const lines[] = {
      what,
      state,
      moved,
      ComputerHasCard() ? "Eject it on the computer first." : "",
  };
  DrawItems(
      menu, "USB disk mode", lines, sizeof(lines) / sizeof(lines[0]),
      kSharingItems, 1);
}

static void DrawResetPage(DisplayMenu* menu) {
  DrawItems(
      menu, "Reset", (const char* const[]){"Reset the VM?"}, 1, kResetItems,
      kResetItemCount);
}

static void DrawPowerOffPage(DisplayMenu* menu) {
  DrawItems(
      menu, "Power off", (const char* const[]){"Power off the VM?"}, 1,
      kPowerOffItems, kPowerOffItemCount);
}

static void DrawUSBPage(DisplayMenu* menu) {
  DrawItems(menu, "USB disk mode", NULL, 0, kUSBItems, kUSBItemCount);
}

static void DrawStopSharingPage(DisplayMenu* menu) {
  DrawItems(
      menu, "USB disk mode",
      (const char* const[]){
          "The computer has not ejected the",
          "disk, and may not have finished",
          "writing to it.",
      },
      3, kStopSharingItems, kStopSharingItemCount);
}

static void DrawSettingsPage(DisplayMenu* menu) {
  DrawItems(menu, "Settings", NULL, 0, kSettingsItems, kSettingsItemCount);
}

static const MenuPageDef kPageDefs[kMenuPageCount] = {
    [kMenuPageMain] = {
        .name = "main",
        .item_count = MainItemCount,
        .draw = DrawMainPage,
        .choose = ChooseMain,
        .back = BackMain,
    },
    [kMenuPageStatus] = {
        .name = "status",
        .hint = "Esc=Back",
        .draw = DrawStatusPage,
        .choose = BackStatus,
        .back = BackStatus,
    },
    [kMenuPageReset] = {
        .name = "reset",
        .item_count = ResetItemCount,
        .draw = DrawResetPage,
        .choose = ChooseReset,
        .back = BackReset,
    },
    [kMenuPagePowerOff] = {
        .name = "power_off",
        .item_count = PowerOffItemCount,
        .draw = DrawPowerOffPage,
        .choose = ChoosePowerOff,
        .back = BackPowerOff,
    },
    [kMenuPageDrive] = {
        .get_name = DrivePageName,
        .item_count = DriveItemCount,
        .draw = DrawDrivePage,
        .choose = ChooseDrive,
        .back = BackDrive,
    },
    [kMenuPageBrowse] = {
        .name = "browse",
        .hint = "\x18\x19=Move  Enter=Open  Backspace=Up a folder  Esc=Back",
        .item_count = BrowseItemCount,
        .draw = DrawBrowsePage,
        .choose = OpenEntry,
        .back = BackBrowse,
    },
    [kMenuPageVMs] = {
        .name = "vms",
        .hint = "\x18\x19=Move  Enter=Select",
        .item_count = VMsItemCount,
        .draw = DrawVMsPage,
        .choose = ChooseVMs,
    },
    [kMenuPageUSB] = {
        .name = "usb",
        .item_count = USBItemCount,
        .draw = DrawUSBPage,
        .choose = ChooseUSB,
        .back = BackUSB,
    },
    [kMenuPageSharing] = {
        .name = "usb_sharing",
        .hint = "Enter=Disconnect",
        .item_count = SharingItemCount,
        .draw = DrawSharingPage,
        .choose = Disconnect,
        .back = Disconnect,
    },
    [kMenuPageStopSharing] = {
        .name = "usb_stop",
        .item_count = StopSharingItemCount,
        .draw = DrawStopSharingPage,
        .choose = ChooseStopSharing,
        .back = BackStopSharing,
    },
    [kMenuPageManage] = {
        .name = "manage",
        .item_count = ManageItemCount,
        .draw = DrawManagePage,
        .choose = ChooseManage,
        .back = BackManage,
    },
    [kMenuPageVM] = {
        .name = "vm",
        .item_count = VMItemCount,
        .draw = DrawVMPage,
        .choose = ChooseVM,
        .back = BackVM,
    },
    [kMenuPageName] = {
        .name = "vm_name",
        .get_hint = NamePageHint,
        .draw = DrawNamePage,
        .choose = ChooseName,
        .back = BackName,
        .handle_key = HandleKeyName,
    },
    [kMenuPageDelete] = {
        .name = "vm_delete",
        .item_count = DeleteItemCount,
        .draw = DrawDeletePage,
        .choose = ChooseDelete,
        .back = BackDelete,
    },
    [kMenuPageCopying] = {
        .name = "vm_copying",
        .hint = "",
        .draw = DrawCopyingPage,
        .handle_key = HandleKeyCopying,
    },
    [kMenuPageNew] = {
        .name = "new_vm",
        .get_hint = NewPageHint,
        .item_count = NewItemCount,
        .draw = DrawNewPage,
        .choose = ChooseNew,
        .back = BackNew,
        .handle_key = HandleKeyNew,
    },
    [kMenuPageNewDrive] = {
        .name = "new_vm_drive",
        .item_count = NewDriveItemCount,
        .draw = DrawNewDrivePage,
        .choose = ChooseNewDriveOption,
        .back = BackNewDrive,
    },
    [kMenuPageSettings] = {
        .name = "settings",
        .item_count = SettingsItemCount,
        .draw = DrawSettingsPage,
        .choose = ChooseSettings,
        .back = BackSettings,
    },
    [kMenuPageFont] = {
        .name = "font",
        .item_count = FontItemCount,
        .draw = DrawFontPage,
        .choose = ChooseFont,
        .back = BackFont,
    },
};

void MenuDraw(uint64_t now_us) {
  if (!g_open) {
    return;
  }
  if ((g_page == kMenuPageStatus || g_page == kMenuPageSharing) &&
      now_us - g_status_drawn_us >= kStatusRefreshUs) {
    g_dirty = true;
  }
  if (!g_dirty) {
    return;
  }
  DisplayMenu* menu = DisplayBeginMenu();
  if (menu == NULL) {
    return;
  }
  const MenuPageDef* def = &kPageDefs[g_page];
  if (def->draw != NULL) {
    def->draw(menu);
  }
  if (g_page == kMenuPageStatus || g_page == kMenuPageSharing) {
    g_status_drawn_us = now_us;
  }
  // A box of another size than the last leaves part of the last showing, so
  // the guest's screen goes under it first. If core 1 cannot take that yet,
  // the box is laid out again next time.
  if (g_box_drawn &&
      (menu->columns != g_drawn_columns || menu->rows != g_drawn_rows) &&
      !g_config.redraw_guest()) {
    return;
  }
  DisplayEndMenu();
  g_drawn_columns = menu->columns;
  g_drawn_rows = menu->rows;
  g_box_drawn = true;
  g_dirty = false;
}
