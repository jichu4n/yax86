#include "menu.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "display.h"
#include "file_list.h"
#include "harddisk.h"
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

  // A page's box: the main page and the question are narrow, centered on the
  // guest's screen, and the status page takes its whole width.
  kGuestScreenRows = 25,
  kNarrowColumns = 40,
  kWideColumns = kTextColumns,
  // A label's column on the status page.
  kStatusLabelColumns = 9,

  // CGA attributes: white on blue for the box, and black on light gray for
  // the item chosen.
  kAttrFrame = 0x1F,
  kAttrText = 0x17,
  kAttrChosen = 0x70,
  kAttrTitle = 0x1E,

  // Code page 437's double lined box.
  kBoxTopLeft = 0xC9,
  kBoxTopRight = 0xBB,
  kBoxBottomLeft = 0xC8,
  kBoxBottomRight = 0xBC,
  kBoxHorizontal = 0xCD,
  kBoxVertical = 0xBA,
  // Code page 437's up and down arrows, which mark more of a list above or
  // below what shows.
  kArrowUp = 0x18,
  kArrowDown = 0x19,

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
} MenuPage;

static const char* const kPageNames[] = {
    [kMenuPageMain] = "main",
    [kMenuPageStatus] = "status",
    [kMenuPageReset] = "reset",
    [kMenuPagePowerOff] = "power_off",
    [kMenuPageDrive] = "drive",
    [kMenuPageBrowse] = "browse",
    [kMenuPageVMs] = "vms",
    [kMenuPageUSB] = "usb",
    [kMenuPageSharing] = "usb_sharing",
    [kMenuPageStopSharing] = "usb_stop",
    [kMenuPageManage] = "manage",
    [kMenuPageVM] = "vm",
    [kMenuPageName] = "vm_name",
    [kMenuPageDelete] = "vm_delete",
    [kMenuPageCopying] = "vm_copying",
};

// An item of a page, and whether choosing it opens another menu, which a
// chevron at its end says.
typedef struct MenuItem {
  const char* text;
  bool opens_menu;
} MenuItem;

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
    [kMainDriveA] = {"Drive A", true},      [kMainDriveB] = {"Drive B", true},
    [kMainStatus] = {"Status", true},       [kMainReset] = {"Reset", false},
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
  kVMRename = 0,
  kVMClone,
  kVMDelete,
  kVMItemCount,
} VMItem;

static const MenuItem kVMItems[kVMItemCount] = {
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

static const MenuItem kManageRow = {"Manage VMs", true};
static const MenuItem kUSBRow = {"USB disk mode", true};

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
// Whether the menu is the list at power on, or a page reached from it, which
// only starting something closes.
static bool g_choosing;
// Whether the browser is finding a disk image to put on USB, from the list,
// rather than a floppy for a drive; and the image found.
static bool g_browsing_for_usb;
static char g_usb_image_path[kFSMaxPath + 1];
static uint32_t g_usb_image_size;
//
// The VM whose page shows, an index in g_vms; the names of the images in its
// drives, empty for none; and whether its vm.cfg read.
static uint8_t g_vm;
static char g_vm_drives[kVMDrives][kDiskImageNameLength + 1];
static bool g_vm_readable;
// The name being typed, and whether it is for a clone rather than a rename.
static char g_name[kFSMaxFolderName + 1];
static uint8_t g_name_length;
static bool g_cloning;
// What deleting the VM would take.
static VMFolderContents g_contents;
// How far a clone has been copied, in KB.
static uint32_t g_copied_kb;
static uint32_t g_copy_kb;
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
  if (g_page == kMenuPageDrive) {
    return g_drive == kFloppyDriveA ? "drive_a" : "drive_b";
  }
  return kPageNames[g_page];
}

static char DriveLetter(FloppyDrive drive) { return (char)('A' + drive); }

static void ShowPage(MenuPage page, uint8_t chosen);
static MenuAction Choose(void);
static MenuAction Back(void);
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
  if (g_page == kMenuPageVMs) {
    return "\x18\x19=Move  Enter=Select";
  }
  if (g_page == kMenuPageSharing) {
    return "Enter=Disconnect";
  }
  if (g_page == kMenuPageStatus) {
    return "Esc=Back";
  }
  if (g_page == kMenuPageName) {
    return g_cloning ? "Enter=Clone  Backspace=Erase  Esc=Cancel"
                     : "Enter=Rename  Backspace=Erase  Esc=Cancel";
  }
  if (g_page == kMenuPageCopying) {
    return "";
  }
  if (g_page == kMenuPageBrowse) {
    return "\x18\x19=Move  Enter=Open  Backspace=Up a folder  Esc=Back";
  }
  return "\x18\x19=Move  Enter=Select  Esc=Back";
}

const char* MenuProblem(void) {
  return g_problem[0] != '\0' ? g_problem : NULL;
}

// Show the browser on a folder, with the image in the drive chosen if it is
// there.
static void Browse(const char* path) {
  // A drive takes a floppy; USB takes either kind of image.
  static const uint32_t kImageSizes[] = {
      kFloppyImageSize, Y_HDC_GEOMETRY_10MB_IMAGE_SIZE};
  FileListRead(&g_list, path, kImageSizes, g_browsing_for_usb ? 2 : 1);
  g_page = kMenuPageBrowse;
  g_chosen = 0;
  g_list_top = 0;
  g_dirty = true;
  // The drives have no images yet at power on.
  if (g_browsing_for_usb) {
    return;
  }
  const DiskImage* image = g_config.drive(g_drive);
  if (image->status != kDiskImageOpen) {
    return;
  }
  char path_of_entry[kFSMaxPath + 1];
  for (uint8_t i = 0; i < g_list.count; ++i) {
    if (g_list.entries[i].kind == kFileListImage &&
        FileListEntryPath(&g_list, &g_list.entries[i], path_of_entry) &&
        FSSamePath(path_of_entry, image->path)) {
      g_chosen = i;
      ScrollToChosen();
    }
  }
}

// The folder the browser starts in: that of the image in the drive, or else
// of the one in the other drive - the next disk of a set is likely beside
// it - or else yax86.
static void StartBrowsing(void) {
  g_browsing_for_usb = false;
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
  if (g_chosen < g_list_top) {
    g_list_top = g_chosen;
  } else if (g_chosen >= g_list_top + kBrowseRows) {
    g_list_top = (uint8_t)(g_chosen - kBrowseRows + 1);
  }
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

static uint8_t MainItemCount(void) {
  return g_config.can_power_off() ? kMainItemCount : kMainPowerOff;
}

static uint8_t DriveItemCount(void) {
  return DriveEmpty() ? kDriveEject : kDriveItemCount;
}

// The list at power on's rows: a VM each, then Manage VMs and USB disk mode.
static uint8_t VMRowCount(void) { return (uint8_t)(g_vms->count + 2); }

static uint8_t ManageRowIndex(void) { return g_vms->count; }

static uint8_t USBRowIndex(void) { return (uint8_t)(g_vms->count + 1); }

// Whether disconnecting would take the card from a computer still using it.
static bool ComputerHasCard(void) {
  const UsbDiskStats stats = UsbDiskGetStats();
  return stats.connected && stats.seen && !stats.ejected;
}

static uint8_t PageItemCount(void) {
  switch (g_page) {
    case kMenuPageMain:
      return MainItemCount();
    case kMenuPageReset:
      return kResetItemCount;
    case kMenuPagePowerOff:
      return kPowerOffItemCount;
    case kMenuPageDrive:
      return DriveItemCount();
    case kMenuPageBrowse:
      return g_list.count;
    case kMenuPageVMs:
      return VMRowCount();
    case kMenuPageUSB:
      return kUSBItemCount;
    case kMenuPageSharing:
      return 1;
    case kMenuPageStopSharing:
      return kStopSharingItemCount;
    case kMenuPageManage:
      return g_vms->count;
    case kMenuPageVM:
      return kVMItemCount;
    case kMenuPageDelete:
      return kDeleteItemCount;
    case kMenuPageName:
    case kMenuPageCopying:
    case kMenuPageStatus:
      return 0;
  }
  return 0;
}

// Show Manage VMs, with the VM at index in the list chosen, or the first if
// there is none there.
static void ShowManaged(uint8_t index) {
  g_list_top = 0;
  ShowPage(kMenuPageManage, index < g_vms->count ? index : 0);
  ScrollToChosen();
}

void MenuShowManagedVMs(const char* name) {
  ShowManaged(VMListFind(g_vms, name));
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
  }
  ShowPage(kMenuPageVM, kVMRename);
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
    ShowPage(kMenuPageCopying, 0);
  }
  g_copied_kb = copied_kb;
  g_copy_kb = total_kb;
  g_dirty = true;
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

// Go back a page, or close the menu from the main one.
static MenuAction Back(void) {
  if (g_page == kMenuPageMain) {
    g_open = false;
    return kMenuActionClosed;
  }
  switch (g_page) {
    case kMenuPageBrowse:
      if (g_browsing_for_usb) {
        ShowPage(kMenuPageUSB, kUSBImage);
      } else {
        ShowPage(kMenuPageDrive, kDriveInsert);
      }
      break;
    case kMenuPageReset:
      ShowPage(kMenuPageMain, kMainReset);
      break;
    case kMenuPagePowerOff:
      ShowPage(kMenuPageMain, kMainPowerOff);
      break;
    case kMenuPageDrive:
      ShowPage(kMenuPageMain, (uint8_t)(kMainDriveA + g_drive));
      break;
    case kMenuPageStatus:
    case kMenuPageMain:
      ShowPage(kMenuPageMain, kMainStatus);
      break;
    case kMenuPageVMs:
      // There is no VM to go back to yet.
      break;
    case kMenuPageUSB:
      ShowPage(kMenuPageVMs, USBRowIndex());
      ScrollToChosen();
      break;
    case kMenuPageSharing:
      return Disconnect();
    case kMenuPageStopSharing:
      ShowPage(kMenuPageSharing, 0);
      break;
    case kMenuPageManage:
      ShowPage(kMenuPageVMs, ManageRowIndex());
      ScrollToChosen();
      break;
    case kMenuPageVM:
      ShowManaged(g_vm);
      break;
    case kMenuPageName:
      ShowPage(kMenuPageVM, g_cloning ? kVMClone : kVMRename);
      break;
    case kMenuPageDelete:
      ShowPage(kMenuPageVM, kVMDelete);
      break;
    case kMenuPageCopying:
      // A copy cannot be stopped part way.
      break;
  }
  return kMenuActionNone;
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
  if (g_browsing_for_usb) {
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

static MenuAction Choose(void) {
  switch (g_page) {
    case kMenuPageMain:
      switch ((MainItem)g_chosen) {
        case kMainDriveA:
        case kMainDriveB:
          g_drive = (FloppyDrive)(g_chosen - kMainDriveA);
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
      break;
    case kMenuPageReset:
      if (g_chosen == kResetConfirm) {
        g_config.reset();
      }
      return Back();
    case kMenuPagePowerOff:
      if (g_chosen == kPowerOffConfirm) {
        g_config.power_off();
      }
      return Back();
    case kMenuPageDrive:
      if (g_chosen == kDriveInsert) {
        StartBrowsing();
      } else {
        g_config.eject_drive(g_drive);
        // Eject goes with the disk.
        ShowPage(kMenuPageDrive, kDriveInsert);
      }
      return kMenuActionNone;
    case kMenuPageBrowse:
      return OpenEntry();
    case kMenuPageVMs:
      if (g_chosen == USBRowIndex()) {
        ShowPage(kMenuPageUSB, kUSBCard);
        return kMenuActionNone;
      }
      if (g_chosen == ManageRowIndex()) {
        ShowManaged(kVMListMax);
        return kMenuActionNone;
      }
      return kMenuActionVMChosen;
    case kMenuPageManage:
      if (g_vms->count > 0) {
        ShowVM(g_chosen);
      }
      return kMenuActionNone;
    case kMenuPageVM:
      if (g_chosen == kVMDelete) {
        VMFolderDescribe(g_vms, &g_vms->vms[g_vm], &g_contents);
        ShowPage(kMenuPageDelete, kDeleteCancel);
      } else {
        StartNaming(g_chosen == kVMClone);
      }
      return kMenuActionNone;
    case kMenuPageName:
      return g_cloning ? kMenuActionCloneVM : kMenuActionRenameVM;
    case kMenuPageDelete:
      if (g_chosen == kDeleteConfirm) {
        return kMenuActionDeleteVM;
      }
      return Back();
    case kMenuPageCopying:
      return kMenuActionNone;
    case kMenuPageUSB:
      if (g_chosen == kUSBImage) {
        g_browsing_for_usb = true;
        Browse("yax86");
        return kMenuActionNone;
      }
      return kMenuActionShareCard;
    case kMenuPageSharing:
      return Disconnect();
    case kMenuPageStopSharing:
      if (g_chosen == kStopSharingConfirm) {
        return kMenuActionStopSharing;
      }
      return Back();
    case kMenuPageStatus:
      return Back();
  }
  return kMenuActionNone;
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
      ShowPage(kMenuPageMain, kMainDriveA);
      return kMenuActionOpened;
    }
    return kMenuActionClosed;
  }
  // A key held down repeats, which moves through the items as a press does.
  if (!g_open || event.state == kSouthbridgeKeyReleased) {
    return kMenuActionNone;
  }
  if (g_page == kMenuPageName) {
    return TypeName(event.key);
  }
  if (g_page == kMenuPageCopying) {
    return kMenuActionNone;
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

// Fill a row of the box: its border, and its inside blank.
static void BlankRow(DisplayTextRow* row, uint8_t columns) {
  row->chars[0] = (char)kBoxVertical;
  row->attrs[0] = kAttrFrame;
  for (uint8_t i = 1; i + 1 < columns; ++i) {
    row->chars[i] = ' ';
    row->attrs[i] = kAttrText;
  }
  row->chars[columns - 1] = (char)kBoxVertical;
  row->attrs[columns - 1] = kAttrFrame;
}

// The top or bottom edge of the box, with a title centered in the top one.
static void EdgeRow(
    DisplayTextRow* row, uint8_t columns, bool top, const char* title) {
  row->chars[0] = (char)(top ? kBoxTopLeft : kBoxBottomLeft);
  for (uint8_t i = 1; i + 1 < columns; ++i) {
    row->chars[i] = (char)kBoxHorizontal;
  }
  row->chars[columns - 1] = (char)(top ? kBoxTopRight : kBoxBottomRight);
  for (uint8_t i = 0; i < columns; ++i) {
    row->attrs[i] = kAttrFrame;
  }
  if (title == NULL) {
    return;
  }
  const uint8_t length = (uint8_t)strlen(title);
  // The title, with a space either side, over the middle of the edge.
  uint8_t column = (uint8_t)((columns - length - 2) / 2);
  row->chars[column] = ' ';
  row->attrs[column++] = kAttrTitle;
  for (uint8_t i = 0; i < length; ++i) {
    row->chars[column] = title[i];
    row->attrs[column++] = kAttrTitle;
  }
  row->chars[column] = ' ';
  row->attrs[column] = kAttrTitle;
}

// Put text inside a row of the box from column, clipped at its border.
static void PutText(
    DisplayTextRow* row, uint8_t columns, uint8_t column, const char* text,
    uint8_t attr) {
  while (*text != '\0' && column + 1 < columns) {
    row->chars[column] = *text++;
    row->attrs[column++] = attr;
  }
}

// An item's row: its text, and the whole width inside the border marked if
// it is the item chosen.
static void ItemRow(
    DisplayTextRow* row, uint8_t columns, const char* text, bool chosen) {
  BlankRow(row, columns);
  if (chosen) {
    for (uint8_t i = 1; i + 1 < columns; ++i) {
      row->attrs[i] = kAttrChosen;
    }
  }
  PutText(row, columns, 3, text, chosen ? kAttrChosen : kAttrText);
}

// A menu item's row, with a chevron near the right border if it opens
// another menu.
static void MenuItemRow(
    DisplayTextRow* row, uint8_t columns, const MenuItem* item, bool chosen) {
  ItemRow(row, columns, item->text, chosen);
  if (item->opens_menu) {
    row->chars[columns - 3] = '>';
  }
}

// Lay out a box of rows, centered on the guest's screen.
static void PlaceBox(DisplayMenu* menu, uint8_t columns, uint8_t rows) {
  menu->columns = columns;
  menu->rows = rows;
  menu->column = (uint8_t)((kTextColumns - columns) / 2);
  menu->row = (uint8_t)((kGuestScreenRows - rows) / 2);
}

// A page of items, under lines of text, if any.
static void DrawItems(
    DisplayMenu* menu, const char* title, const char* const* lines,
    uint8_t line_count, const MenuItem* items, uint8_t count) {
  const uint8_t text_rows = line_count > 0 ? (uint8_t)(line_count + 1) : 0;
  // The edges, a blank row either side of the items, and the lines with a
  // blank row under them - or at power on, the list's size, as there is no
  // guest's screen to draw under a smaller box to hide the list's.
  uint8_t rows = (uint8_t)(count + 4 + text_rows);
  if (g_choosing && rows < kBrowseRows + 2) {
    rows = kBrowseRows + 2;
  }
  PlaceBox(menu, kNarrowColumns, rows);
  uint8_t row = 0;
  EdgeRow(&menu->text[row++], menu->columns, true, title);
  BlankRow(&menu->text[row++], menu->columns);
  for (uint8_t i = 0; i < line_count; ++i) {
    BlankRow(&menu->text[row], menu->columns);
    PutText(&menu->text[row++], menu->columns, 3, lines[i], kAttrText);
  }
  if (line_count > 0) {
    BlankRow(&menu->text[row++], menu->columns);
  }
  for (uint8_t i = 0; i < count; ++i) {
    MenuItemRow(&menu->text[row++], menu->columns, &items[i], i == g_chosen);
  }
  while (row + 1 < rows) {
    BlankRow(&menu->text[row++], menu->columns);
  }
  EdgeRow(&menu->text[row], menu->columns, false, NULL);
}

static void DrawStatusPage(DisplayMenu* menu) {
  MenuStatusLine lines[kMenuStatusLines];
  g_config.describe_status(lines);
  PlaceBox(menu, kWideColumns, kMenuStatusLines + 4);
  uint8_t row = 0;
  EdgeRow(&menu->text[row++], menu->columns, true, "Status");
  BlankRow(&menu->text[row++], menu->columns);
  for (uint8_t i = 0; i < kMenuStatusLines; ++i) {
    DisplayTextRow* text = &menu->text[row++];
    BlankRow(text, menu->columns);
    PutText(text, menu->columns, 2, lines[i].label, kAttrTitle);
    PutText(
        text, menu->columns, 2 + kStatusLabelColumns, lines[i].value,
        kAttrText);
  }
  BlankRow(&menu->text[row++], menu->columns);
  EdgeRow(&menu->text[row], menu->columns, false, NULL);
}

static void DrawDrivePage(DisplayMenu* menu) {
  const DiskImage* image = g_config.drive(g_drive);
  char line[kNarrowColumns];
  const char* in_drive = "the built-in boot floppy";
  char name[kDiskImageNameLength + 1];
  const bool empty = DriveEmpty();
  if (image->status == kDiskImageOpen) {
    DiskImageName(image->path, name);
    in_drive = name;
  } else if (empty) {
    in_drive = "nothing";
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
  const char title[] = {'D', 'r', 'i', 'v', 'e', ' ', DriveLetter(g_drive),
                        '\0'};
  DrawItems(
      menu, title, lines, sizeof(lines) / sizeof(lines[0]), items,
      DriveItemCount());
}

// The browser: the folder as the title, and a row for each entry that
// shows, with arrows on the border where there is more above or below.
static void DrawBrowsePage(DisplayMenu* menu) {
  PlaceBox(menu, kNarrowColumns, kBrowseRows + 2);
  // The folder, from the card's root, keeping its end if it is too long.
  char title[kNarrowColumns - 6 + 1];
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
  EdgeRow(&menu->text[0], menu->columns, true, title);
  for (uint8_t i = 0; i < kBrowseRows; ++i) {
    DisplayTextRow* row = &menu->text[1 + i];
    const uint8_t index = (uint8_t)(g_list_top + i);
    if (index >= g_list.count) {
      BlankRow(row, menu->columns);
      if (i == 0 && !g_list.readable) {
        PutText(row, menu->columns, 3, "The card cannot be read", kAttrText);
      } else if (i == 1 && g_list.count <= 1) {
        PutText(
            row, menu->columns, 3,
            g_browsing_for_usb ? "No folders or disk images"
                               : "No folders or floppy images",
            kAttrText);
      }
      continue;
    }
    const FileListEntry* entry = &g_list.entries[index];
    ItemRow(row, menu->columns, entry->name, index == g_chosen);
    const uint8_t attr = index == g_chosen ? kAttrChosen : kAttrText;
    char path[kFSMaxPath + 1];
    if (entry->kind != kFileListImage) {
      PutText(row, menu->columns, kBrowseNoteColumn, "<DIR>", attr);
      continue;
    }
    if (g_browsing_for_usb) {
      PutText(
          row, menu->columns, kBrowseNoteColumn,
          entry->size == kFloppyImageSize ? "floppy" : "hard disk", attr);
      continue;
    }
    if (!FileListEntryPath(&g_list, entry, path)) {
      continue;
    }
    for (uint8_t drive = 0; drive < kFloppyDrives; ++drive) {
      const DiskImage* image = g_config.drive((FloppyDrive)drive);
      if (image->status == kDiskImageOpen && FSSamePath(path, image->path)) {
        char note[] = "in drive A";
        note[sizeof(note) - 2] = DriveLetter((FloppyDrive)drive);
        PutText(row, menu->columns, kBrowseNoteColumn, note, attr);
      }
    }
  }
  // A folder with more entries than a list holds says so on the bottom edge.
  EdgeRow(
      &menu->text[kBrowseRows + 1], menu->columns, false,
      g_list.truncated ? "the first 64 entries" : NULL);
  if (g_list_top > 0) {
    menu->text[1].chars[menu->columns - 1] = (char)kArrowUp;
  }
  if (g_list_top + kBrowseRows < g_list.count) {
    menu->text[kBrowseRows].chars[menu->columns - 1] = (char)kArrowDown;
  }
}

// The VMs to start: a row each, with arrows on the border where there
// are more above or below, and the countdown on the bottom edge.
static void DrawVMsPage(DisplayMenu* menu) {
  PlaceBox(menu, kNarrowColumns, kBrowseRows + 2);
  EdgeRow(&menu->text[0], menu->columns, true, "YAX86");
  for (uint8_t i = 0; i < kBrowseRows; ++i) {
    DisplayTextRow* row = &menu->text[1 + i];
    const uint8_t index = (uint8_t)(g_list_top + i);
    if (index >= VMRowCount()) {
      BlankRow(row, menu->columns);
    } else if (index == ManageRowIndex()) {
      MenuItemRow(row, menu->columns, &kManageRow, index == g_chosen);
    } else if (index == USBRowIndex()) {
      MenuItemRow(row, menu->columns, &kUSBRow, index == g_chosen);
    } else {
      ItemRow(row, menu->columns, g_vms->vms[index].name, index == g_chosen);
    }
  }
  char countdown[kNarrowColumns];
  if (g_countdown > 0) {
    snprintf(
        countdown, sizeof(countdown), "starting in %u s",
        (unsigned)g_countdown);
  }
  EdgeRow(
      &menu->text[kBrowseRows + 1], menu->columns, false,
      g_countdown > 0 ? countdown : NULL);
  if (g_list_top > 0) {
    menu->text[1].chars[menu->columns - 1] = (char)kArrowUp;
  }
  if (g_list_top + kBrowseRows < VMRowCount()) {
    menu->text[kBrowseRows].chars[menu->columns - 1] = (char)kArrowDown;
  }
}

// Manage VMs: the VMs in yax86/vm, a row each, with arrows on the border
// where there are more above or below.
static void DrawManagePage(DisplayMenu* menu) {
  PlaceBox(menu, kNarrowColumns, kBrowseRows + 2);
  EdgeRow(&menu->text[0], menu->columns, true, "Manage VMs");
  for (uint8_t i = 0; i < kBrowseRows; ++i) {
    DisplayTextRow* row = &menu->text[1 + i];
    const uint8_t index = (uint8_t)(g_list_top + i);
    if (index < g_vms->count) {
      const MenuItem item = {g_vms->vms[index].name, true};
      MenuItemRow(row, menu->columns, &item, index == g_chosen);
      continue;
    }
    BlankRow(row, menu->columns);
    if (i == 0 && g_vms->count == 0) {
      PutText(row, menu->columns, 3, "No VMs in /YAX86/VM", kAttrText);
    }
  }
  EdgeRow(&menu->text[kBrowseRows + 1], menu->columns, false, NULL);
  if (g_list_top > 0) {
    menu->text[1].chars[menu->columns - 1] = (char)kArrowUp;
  }
  if (g_list_top + kBrowseRows < g_vms->count) {
    menu->text[kBrowseRows].chars[menu->columns - 1] = (char)kArrowDown;
  }
}

// A VM's page: the images in its drives, over what can be done to it.
static void DrawVMPage(DisplayMenu* menu) {
  // Room for any name, which the box cuts to its width.
  char lines[kVMDrives][64];
  const char* line_pointers[kVMDrives];
  uint8_t line_count = 0;
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    if (g_vm_drives[drive][0] != '\0') {
      snprintf(
          lines[line_count], sizeof(lines[line_count]), "%c: %s",
          (char)('A' + drive), g_vm_drives[drive]);
      line_pointers[line_count] = lines[line_count];
      ++line_count;
    }
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
  char title[kNarrowColumns];
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
  PutText(row, menu->columns, 3, field, kAttrChosen);
}

// Join names into text as "A, B and C", for as many as fit, then "and N
// more" for the rest.
static void JoinNames(
    char* text, uint8_t size, char (*names)[kFSMaxName + 1], uint8_t named,
    uint8_t count) {
  uint8_t length = 0;
  text[0] = '\0';
  for (uint8_t i = 0; i < named; ++i) {
    const char* separator = i == 0 ? "" : (i + 1 == count ? " and " : ", ");
    const int written = snprintf(
        text + length, (size_t)(size - length), "%s%s", separator, names[i]);
    if (written < 0 || length + written >= size) {
      text[length] = '\0';
      break;
    }
    length = (uint8_t)(length + written);
  }
  if (named < count) {
    snprintf(
        text + length, (size_t)(size - length), " and %u more",
        (unsigned)(count - named));
  }
}

// The question before deleting a VM, saying what goes with it, and which VMs
// lose a disk.
static void DrawDeletePage(DisplayMenu* menu) {
  const uint8_t named = g_contents.file_count < kVMFolderNamed
                            ? g_contents.file_count
                            : kVMFolderNamed;
  char question[kNarrowColumns];
  snprintf(question, sizeof(question), "Delete %s?", g_vms->vms[g_vm].name);
  char files[kNarrowColumns - 4];
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

static void DrawCopyingPage(DisplayMenu* menu) {
  char copying[kNarrowColumns];
  snprintf(
      copying, sizeof(copying), "Copying %s to %s...", g_vms->vms[g_vm].name,
      g_name);
  char progress[kNarrowColumns];
  snprintf(
      progress, sizeof(progress), "%lu of %lu KB", (unsigned long)g_copied_kb,
      (unsigned long)g_copy_kb);
  const char* const lines[] = {copying, progress};
  DrawItems(menu, "Clone", lines, 2, NULL, 0);
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
  char what[kNarrowColumns];
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
  switch (g_page) {
    case kMenuPageMain:
      DrawItems(menu, "yax86", NULL, 0, kMainItems, MainItemCount());
      break;
    case kMenuPageReset:
      DrawItems(
          menu, "Reset", (const char* const[]){"Reset the VM?"}, 1, kResetItems,
          kResetItemCount);
      break;
    case kMenuPagePowerOff:
      DrawItems(
          menu, "Power off", (const char* const[]){"Power off the VM?"}, 1,
          kPowerOffItems, kPowerOffItemCount);
      break;
    case kMenuPageDrive:
      DrawDrivePage(menu);
      break;
    case kMenuPageBrowse:
      DrawBrowsePage(menu);
      break;
    case kMenuPageVMs:
      DrawVMsPage(menu);
      break;
    case kMenuPageUSB:
      DrawItems(menu, "USB disk mode", NULL, 0, kUSBItems, kUSBItemCount);
      break;
    case kMenuPageSharing:
      DrawSharingPage(menu);
      g_status_drawn_us = now_us;
      break;
    case kMenuPageStopSharing:
      DrawItems(
          menu, "USB disk mode",
          (const char* const[]){
              "The computer has not ejected the",
              "disk, and may not have finished",
              "writing to it.",
          },
          3, kStopSharingItems, kStopSharingItemCount);
      break;
    case kMenuPageStatus:
      DrawStatusPage(menu);
      g_status_drawn_us = now_us;
      break;
    case kMenuPageManage:
      DrawManagePage(menu);
      break;
    case kMenuPageVM:
      DrawVMPage(menu);
      break;
    case kMenuPageName:
      DrawNamePage(menu);
      break;
    case kMenuPageDelete:
      DrawDeletePage(menu);
      break;
    case kMenuPageCopying:
      DrawCopyingPage(menu);
      break;
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
