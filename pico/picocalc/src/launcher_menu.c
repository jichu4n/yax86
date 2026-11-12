#include "launcher_menu.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "display.h"
#include "ui_box.h"
#include "ui_browser.h"
#include "ui_page.h"
#include "ui_status.h"
#include "usb_disk.h"

enum {
  kStatusRefreshUs = 1000 * 1000,
};

typedef enum ListPage {
  // The VMs to start, and from there what can go on USB, the card while it
  // is there, and the question before taking it back.
  kListPageVMs = 0,
  kListPageUSB,
  kListPageSharing,
  kListPageStopSharing,
  // Manage VMs, from the list: the VMs in yax86/vm, the page of the one in
  // g_vm, the name typed to rename or clone it as, the question before
  // deleting it, and a clone being copied.
  kListPageManage,
  kListPageVM,
  kListPageName,
  kListPageDelete,
  kListPageCopying,
  // New VM, from Manage VMs: its form, and the choice for one of its drives,
  // g_new_drive.
  kListPageNew,
  kListPageNewDrive,
  // Settings, from the list, and Font, from Settings: the 4x10 fonts.
  kListPageSettings,
  kListPageFont,
  // The browser, finding an image to put on USB or for a new VM's drive.
  kListPageBrowse,
} ListPage;

static const char* const kPageNames[] = {
    [kListPageVMs] = "vms",
    [kListPageUSB] = "usb",
    [kListPageSharing] = "usb_sharing",
    [kListPageStopSharing] = "usb_stop",
    [kListPageManage] = "manage",
    [kListPageVM] = "vm",
    [kListPageName] = "vm_name",
    [kListPageDelete] = "vm_delete",
    [kListPageCopying] = "vm_copying",
    [kListPageNew] = "new_vm",
    [kListPageNewDrive] = "new_vm_drive",
    [kListPageSettings] = "settings",
    [kListPageFont] = "font",
    [kListPageBrowse] = "browse",
};

typedef enum USBItem {
  kUSBCard = 0,
  kUSBImage,
  kUSBItemCount,
} USBItem;

static const UIBoxItem kUSBItems[kUSBItemCount] = {
    [kUSBCard] = {"Whole SD card", false},
    [kUSBImage] = {"Disk image", true},
};

static const UIBoxItem kSharingItems[] = {{"Disconnect", false}};

typedef enum StopSharingItem {
  kStopSharingCancel = 0,
  kStopSharingConfirm,
  kStopSharingItemCount,
} StopSharingItem;

static const UIBoxItem kStopSharingItems[kStopSharingItemCount] = {
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

static const UIBoxItem kVMItems[kVMItemCount] = {
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

static const UIBoxItem kDeleteItems[kDeleteItemCount] = {
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

static const UIBoxItem kNewFloppyItems[kNewDriveItemCount] = {
    [kNewDriveDisabled] = {"Disabled", false},
    [kNewDriveNone] = {"Empty", false},
    [kNewDriveBlank] = {"New floppy image (360KB)", false},
    [kNewDriveImage] = {"Existing floppy image", true},
};

static const UIBoxItem kNewHardDiskItems[kNewDriveItemCount] = {
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

static const UIBoxItem kNewVMRow = {"New VM", true};
static const UIBoxItem kManageRow = {"Manage VMs", true};
static const UIBoxItem kUSBRow = {"USB disk mode", true};
static const UIBoxItem kSettingsRow = {"Settings", true};
static const UIBoxItem kSettingsItems[] = {{"Font", true}};
enum {
  kSettingsItemCount = sizeof(kSettingsItems) / sizeof(kSettingsItems[0]),
};

static ListPage g_page;
static uint8_t g_chosen;
// Whether what is on the panel is out of date, and when the sharing page was
// last drawn.
static bool g_dirty;
static uint64_t g_status_drawn_us;
// The row at the top of the list at power on, and of Manage VMs.
static uint8_t g_list_top;
// The VMs to choose from at power on, the seconds before the
// highlighted one starts by itself, and why the last chosen did not.
static const VMList* g_vms;
static uint8_t g_countdown;
static Font4x10 g_font = kFont4x10Default;
// What the browser is finding an image for: an image of either kind to put
// on USB, or an image for a new VM's drive.
typedef enum BrowsePurpose {
  kBrowseForUSB = 0,
  kBrowseForNewVM,
} BrowsePurpose;
static BrowsePurpose g_browsing_for;
static UIBrowser g_browser;
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
static VMListContents g_contents;
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
const char* LauncherMenuPageName(void) { return kPageNames[g_page]; }

static void ShowPage(ListPage page, uint8_t chosen);
static LauncherMenuAction Choose(void);
static LauncherMenuAction Back(void);
static void ShowVM(uint8_t index);
static uint8_t FormFirstItem(void);
static uint8_t FormItem(void);
static const NewDriveItem* DriveChoices(uint8_t* count);
static uint8_t DriveChoiceRow(NewDriveItem choice);
static void ScrollToChosen(void);

void LauncherMenuOpen(const VMList* list, uint8_t highlighted) {
  g_vms = list;
  g_list_top = 0;
  ShowPage(kListPageVMs, highlighted);
  ScrollToChosen();
}

uint8_t LauncherMenuChosenVM(void) { return g_chosen; }

Font4x10 LauncherMenuChosenFont(void) { return g_font; }

void LauncherMenuSetFont(Font4x10 font) {
  if (font != g_font) {
    g_font = font;
    g_dirty = true;
  }
}

void LauncherMenuShowSharing(void) { ShowPage(kListPageSharing, 0); }

void LauncherMenuSetCountdown(uint8_t seconds) {
  if (seconds != g_countdown) {
    g_countdown = seconds;
    g_dirty = true;
  }
}

const char* LauncherMenuHint(void) {
  if (g_page == kListPageVMs) {
    return "\x18\x19=Move  Enter=Select";
  }
  if (g_page == kListPageSharing) {
    return "Enter=Disconnect";
  }
  if (g_page == kListPageName) {
    return g_cloning ? "Enter=Clone  Backspace=Erase  Esc=Cancel"
                     : "Enter=Rename  Backspace=Erase  Esc=Cancel";
  }
  if (g_page == kListPageCopying) {
    return "";
  }
  if (g_page == kListPageNew) {
    return FormItem() == kNewName
               ? "Type the name  \x18\x19=Move  Backspace=Erase  Esc=Cancel"
               : "\x18\x19=Move  Enter=Select  Esc=Cancel";
  }
  if (g_page == kListPageBrowse) {
    return "\x18\x19=Move  Enter=Open  Backspace=Up a folder  Esc=Back";
  }
  return "\x18\x19=Move  Enter=Select  Esc=Back";
}

// Show the browser on a folder, with the image in the drive chosen if it is
// there.
static void Browse(const char* folder) {
  ShowPage(kListPageBrowse, UIBrowserOpen(&g_browser, folder));
}

// Set the browser up to find an image for purpose: the images it lists, and
// the one already chosen for the drive.
static void StartBrowser(BrowsePurpose purpose) {
  g_browsing_for = purpose;
  g_browser.images = kUIBrowserFloppies;
  g_browser.current = NULL;
  for (uint8_t drive = 0; drive < kFloppyDrives; ++drive) {
    g_browser.in_drives[drive] = NULL;
  }
  if (purpose == kBrowseForUSB) {
    g_browser.images = kUIBrowserEither;
  } else {
    if (g_new_drive >= kVMDriveHD1) {
      g_browser.images = kUIBrowserHardDisks;
    }
    g_browser.current = g_new_drives[g_new_drive];
  }
}

// Keep the chosen entry among the browser's rows.
static void ScrollToChosen(void) {
  if (g_page == kListPageBrowse) {
    UIBrowserScrollTo(&g_browser, g_chosen);
    return;
  }
  uint8_t row = g_chosen;
  if ((g_page == kListPageVMs || g_page == kListPageManage) &&
      g_vms->count > 0 && g_chosen >= g_vms->count) {
    ++row;
  }
  UIPageScrollTo(row, &g_list_top);
}

// Show the browser on the folder above, with the folder just left chosen.
static void BrowseUp(void) {
  ShowPage(kListPageBrowse, UIBrowserUp(&g_browser));
}

static void ShowPage(ListPage page, uint8_t chosen) {
  g_page = page;
  g_chosen = chosen;
  g_dirty = true;
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

static uint8_t PageItemCount(void) {
  switch (g_page) {
    case kListPageBrowse:
      return UIBrowserCount(&g_browser);
    case kListPageVMs:
      return VMsItemCount();
    case kListPageUSB:
      return kUSBItemCount;
    case kListPageSharing:
      return 1;
    case kListPageStopSharing:
      return kStopSharingItemCount;
    case kListPageManage:
      return (uint8_t)(g_vms->count + 1);
    case kListPageNew:
      return (uint8_t)(kNewItemCount - FormFirstItem());
    case kListPageNewDrive:
      return DriveChoiceRow(kNewDriveItemCount);
    case kListPageSettings:
      return kSettingsItemCount;
    case kListPageFont:
      return kFont4x10Count;
    case kListPageVM:
      return kVMItemCount;
    case kListPageDelete:
      return kDeleteItemCount;
    case kListPageName:
    case kListPageCopying:
      return 0;
  }
  return 0;
}

// Show Manage VMs - a row for each VM, then New VM - with the item at index
// chosen.
static void ShowManaged(uint8_t index) {
  g_list_top = 0;
  ShowPage(kListPageManage, index);
  ScrollToChosen();
}

void LauncherMenuShowManagedVMs(const char* name, bool show_vm) {
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
  ShowPage(kListPageVM, kVMEdit);
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
  ShowPage(kListPageName, 0);
}

// A key on the name's page: a character a name can have to add, Backspace to
// take the last off, Enter to go ahead, and Esc to go back.
static LauncherMenuAction TypeName(uint8_t key) {
  switch (key) {
    case kKeyCodeEnter:
      return Choose();
    case kKeyCodeEsc:
      return Back();
    case kKeyCodeBackspace:
      if (g_name_length > 0) {
        g_name[--g_name_length] = '\0';
        g_dirty = true;
      }
      return kLauncherMenuActionNone;
    default:
      break;
  }
  if (key < 0x80 && FSIsFolderNameCharacter((char)key) &&
      g_name_length < kFSMaxFolderName) {
    g_name[g_name_length++] = (char)toupper(key);
    g_name[g_name_length] = '\0';
    g_dirty = true;
  }
  return kLauncherMenuActionNone;
}

uint8_t LauncherMenuManagedVM(void) { return g_vm; }

const char* LauncherMenuTypedName(void) { return g_name; }

void LauncherMenuSetCopyProgress(uint32_t copied_kb, uint32_t total_kb) {
  if (g_page != kListPageCopying) {
    g_creating = g_page == kListPageNew;
    ShowPage(kListPageCopying, 0);
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
  ShowPage(kListPageNew, (uint8_t)(item - FormFirstItem()));
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
  ShowPage(kListPageNewDrive, DriveChoiceRow((NewDriveItem)chosen));
}

// Take a choice for a drive on the form, back to the form; or for an image,
// find it in the browser, starting where the one chosen is, if one is.
static void ChooseNewDrive(void) {
  uint8_t count = 0;
  const NewDriveItem choice = DriveChoices(&count)[g_chosen];
  char* path = g_new_drives[g_new_drive];
  if (choice == kNewDriveImage) {
    StartBrowser(kBrowseForNewVM);
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
  if (key == kKeyCodeBackspace) {
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

void LauncherMenuFormDrives(VMListDrives* drives) {
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    drives->paths[drive] = g_new_present[drive] ? g_new_drives[drive] : NULL;
    drives->blank[drive] = g_new_blank[drive];
  }
}

// Disconnect the card: at once if no computer is using it, and after asking if
// one is.
static LauncherMenuAction Disconnect(void) {
  if (ComputerHasCard()) {
    ShowPage(kListPageStopSharing, kStopSharingCancel);
    return kLauncherMenuActionNone;
  }
  return kLauncherMenuActionStopSharing;
}

// Go back a page.
static LauncherMenuAction Back(void) {
  switch (g_page) {
    case kListPageBrowse:
      if (g_browsing_for == kBrowseForUSB) {
        ShowPage(kListPageUSB, kUSBImage);
      } else {
        ShowPage(kListPageNewDrive, DriveChoiceRow(kNewDriveImage));
      }
      break;
    case kListPageVMs:
      // There is no VM to go back to yet.
      break;
    case kListPageUSB:
      ShowPage(kListPageVMs, USBItemIndex());
      ScrollToChosen();
      break;
    case kListPageSharing:
      return Disconnect();
    case kListPageStopSharing:
      ShowPage(kListPageSharing, 0);
      break;
    case kListPageManage:
      ShowPage(kListPageVMs, ManageItemIndex());
      ScrollToChosen();
      break;
    case kListPageVM:
      ShowManaged(g_vm);
      break;
    case kListPageName:
      ShowPage(kListPageVM, g_cloning ? kVMClone : kVMRename);
      break;
    case kListPageDelete:
      ShowPage(kListPageVM, kVMDelete);
      break;
    case kListPageCopying:
      // A copy cannot be stopped part way.
      break;
    case kListPageNew:
      if (g_editing) {
        ShowPage(kListPageVM, kVMEdit);
      } else {
        ShowManaged(g_vms->count);
      }
      break;
    case kListPageNewDrive:
      ShowFormItem((uint8_t)(kNewDriveA + g_new_drive));
      break;
    case kListPageSettings:
      ShowPage(kListPageVMs, SettingsItemIndex());
      ScrollToChosen();
      break;
    case kListPageFont:
      ShowPage(kListPageSettings, 0);
      break;
  }
  return kLauncherMenuActionNone;
}

// Open the chosen entry of the browser: a folder to look in, or an image for
// a new VM's drive or to put on USB.
static LauncherMenuAction OpenEntry(void) {
  uint8_t chosen = g_chosen;
  char path[kFSMaxPath + 1];
  uint32_t size = 0;
  switch (UIBrowserChoose(&g_browser, &chosen, path, &size)) {
    case kUIBrowserNothing:
      return kLauncherMenuActionNone;
    case kUIBrowserOpened:
      ShowPage(kListPageBrowse, chosen);
      return kLauncherMenuActionNone;
    case kUIBrowserChoseImage:
      break;
  }
  if (g_browsing_for == kBrowseForNewVM) {
    FSCopyPath(path, g_new_drives[g_new_drive]);
    g_new_blank[g_new_drive] = false;
    g_new_present[g_new_drive] = true;
    ShowFormItem((uint8_t)(kNewDriveA + g_new_drive));
    return kLauncherMenuActionNone;
  }
  FSCopyPath(path, g_usb_image_path);
  g_usb_image_size = size;
  return kLauncherMenuActionShareImage;
}

const char* LauncherMenuChosenImage(uint32_t* size) {
  *size = g_usb_image_size;
  return g_usb_image_path;
}

static LauncherMenuAction Choose(void) {
  switch (g_page) {
    case kListPageBrowse:
      return OpenEntry();
    case kListPageVMs:
      if (g_chosen == SettingsItemIndex()) {
        ShowPage(kListPageSettings, 0);
        return kLauncherMenuActionNone;
      }
      if (g_chosen == USBItemIndex()) {
        ShowPage(kListPageUSB, kUSBCard);
        return kLauncherMenuActionNone;
      }
      if (g_chosen == ManageItemIndex()) {
        ShowManaged(0);
        return kLauncherMenuActionNone;
      }
      return kLauncherMenuActionVMChosen;
    case kListPageSettings:
      ShowPage(kListPageFont, g_font);
      return kLauncherMenuActionNone;
    case kListPageFont:
      g_font = (Font4x10)g_chosen;
      ShowPage(kListPageSettings, 0);
      return kLauncherMenuActionFontChosen;
    case kListPageManage:
      if (g_chosen == g_vms->count) {
        StartNewVM();
      } else {
        ShowVM(g_chosen);
      }
      return kLauncherMenuActionNone;
    case kListPageNew:
      if (FormItem() == kNewCreate) {
        return g_editing ? kLauncherMenuActionEditVM
                         : kLauncherMenuActionCreateVM;
      }
      if (FormItem() == kNewName) {
        ShowFormItem(kNewDriveA);
      } else {
        ShowNewDrive((uint8_t)(FormItem() - kNewDriveA));
      }
      return kLauncherMenuActionNone;
    case kListPageNewDrive:
      ChooseNewDrive();
      return kLauncherMenuActionNone;
    case kListPageVM:
      if (g_chosen == kVMEdit) {
        StartEditVM();
      } else if (g_chosen == kVMDelete) {
        VMListDescribe(g_vms, &g_vms->vms[g_vm], &g_contents);
        ShowPage(kListPageDelete, kDeleteCancel);
      } else {
        StartNaming(g_chosen == kVMClone);
      }
      return kLauncherMenuActionNone;
    case kListPageName:
      return g_cloning ? kLauncherMenuActionCloneVM
                       : kLauncherMenuActionRenameVM;
    case kListPageDelete:
      if (g_chosen == kDeleteConfirm) {
        return kLauncherMenuActionDeleteVM;
      }
      return Back();
    case kListPageCopying:
      return kLauncherMenuActionNone;
    case kListPageUSB:
      if (g_chosen == kUSBImage) {
        StartBrowser(kBrowseForUSB);
        Browse("yax86");
        return kLauncherMenuActionNone;
      }
      return kLauncherMenuActionShareCard;
    case kListPageSharing:
      return Disconnect();
    case kListPageStopSharing:
      if (g_chosen == kStopSharingConfirm) {
        return kLauncherMenuActionStopSharing;
      }
      return Back();
  }
  return kLauncherMenuActionNone;
}

LauncherMenuAction LauncherMenuHandleKey(SouthbridgeKeyEvent event) {
  // Nothing closes the list but starting something, and Esc goes back no
  // further than it.
  if (g_page == kListPageVMs &&
      (event.key == kKeyCodeEsc || event.key == kKeyCodeBackspace)) {
    return kLauncherMenuActionNone;
  }
  if (event.state != kSouthbridgeKeyReleased && UIStatusClearProblem()) {
    g_dirty = true;
  }
  // A key held down repeats, which moves through the items as a press does.
  if (event.state == kSouthbridgeKeyReleased) {
    return kLauncherMenuActionNone;
  }
  if (g_page == kListPageName) {
    return TypeName(event.key);
  }
  if (g_page == kListPageCopying) {
    return kLauncherMenuActionNone;
  }
  if (g_page == kListPageNew) {
    // Backspace erases the name rather than leaving the form, wherever it
    // is: Esc leaves it.
    if ((FormItem() == kNewName && TypeNewName(event.key)) ||
        event.key == kKeyCodeBackspace) {
      return kLauncherMenuActionNone;
    }
  }
  if (UIPageIsMoveKey(event.key)) {
    if (UIPageMove(event.key, PageItemCount(), &g_chosen)) {
      g_dirty = true;
    }
    ScrollToChosen();
    return kLauncherMenuActionNone;
  }
  switch (event.key) {
    case kKeyCodeEnter:
    case kKeyCodeSpace:
      return Choose();
    case kKeyCodeBackspace:
      // Up a folder in the browser, until the card's root.
      if (g_page == kListPageBrowse && !UIBrowserAtRoot(&g_browser)) {
        BrowseUp();
        return kLauncherMenuActionNone;
      }
      return Back();
    case kKeyCodeEsc:
      return Back();
    default:
      return kLauncherMenuActionNone;
  }
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

// Manage VMs: the VMs in yax86/vm, a row each, then New VM after a line if
// there are any, with arrows on the border where there are more above or
// below.
static void DrawManagePage(DisplayMenu* menu) {
  UIBoxPlace(menu, kUIPageNarrowColumns, kUIPageListRows + 2);
  UIBoxEdgeRow(&menu->text[0], menu->columns, true, "Manage VMs");
  for (uint8_t i = 0; i < kUIPageListRows; ++i) {
    DisplayTextRow* row = &menu->text[1 + i];
    const uint8_t index = (uint8_t)(g_list_top + i);
    if (index >= ManageRowCount()) {
      UIBoxBlankRow(row, menu->columns);
    } else if (index < g_vms->count) {
      const UIBoxItem item = {g_vms->vms[index].name, true};
      UIBoxMenuItemRow(row, menu->columns, &item, index == g_chosen);
    } else if (g_vms->count > 0 && index == g_vms->count) {
      UIBoxSeparatorRow(row, menu->columns);
    } else {
      UIBoxMenuItemRow(
          row, menu->columns, &kNewVMRow, g_chosen == g_vms->count);
    }
  }
  UIBoxEdgeRow(&menu->text[kUIPageListRows + 1], menu->columns, false, NULL);
  UIPageDrawScrollArrows(menu, g_list_top, ManageRowCount());
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
  UIPageDrawItems(
      menu, g_vms->vms[g_vm].name, line_pointers, line_count, kVMItems,
      kVMItemCount, g_chosen, true);
}

// The name being typed, in a field marked as the item chosen is, with a
// cursor after it while there is room.
static void DrawNamePage(DisplayMenu* menu) {
  char title[kUIPageNarrowColumns];
  snprintf(
      title, sizeof(title), "%s %s", g_cloning ? "Clone" : "Rename",
      g_vms->vms[g_vm].name);
  const char* const lines[] = {
      g_cloning ? "Name of the copy:" : "New name:",
      "",
      "",
      "Up to 8 letters, digits, - and _.",
  };
  UIPageDrawItems(
      menu, title, lines, sizeof(lines) / sizeof(lines[0]), NULL, 0, g_chosen,
      true);
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
  const uint8_t named = g_contents.file_count < kVMListNamed
                            ? g_contents.file_count
                            : kVMListNamed;
  char question[kUIPageNarrowColumns];
  snprintf(question, sizeof(question), "Delete %s?", g_vms->vms[g_vm].name);
  char files[kUIPageNarrowColumns - 4];
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
  UIPageDrawItems(
      menu, "Delete", lines, line_count, kDeleteItems, kDeleteItemCount,
      g_chosen, true);
}

// A clone being copied, or a new VM's blank disks being written.
static void DrawCopyingPage(DisplayMenu* menu) {
  char copying[kUIPageNarrowColumns];
  if (g_creating) {
    snprintf(
        copying, sizeof(copying), "Writing %s's blank disks...",
        g_editing ? g_vms->vms[g_vm].name : g_name);
  } else {
    snprintf(
        copying, sizeof(copying), "Copying %s to %s...", g_vms->vms[g_vm].name,
        g_name);
  }
  char progress[kUIPageNarrowColumns];
  snprintf(
      progress, sizeof(progress), "%lu of %lu KB", (unsigned long)g_copied_kb,
      (unsigned long)g_copy_kb);
  const char* const lines[] = {copying, progress};
  UIPageDrawItems(
      menu, g_creating ? (g_editing ? "Edit drives" : "New VM") : "Clone",
      lines, 2, NULL, 0, g_chosen, true);
}

// The form: a new VM's name, typed in place with a cursor while it is
// chosen, and the drives, with what each holds - or editing a VM's drives,
// the drives alone, under its name.
static void DrawNewPage(DisplayMenu* menu) {
  char rows[kNewItemCount][kUIPageNarrowColumns];
  UIBoxItem items[kNewItemCount];
  const char* cursor =
      FormItem() == kNewName && g_name_length < kFSMaxFolderName ? "_" : "";
  snprintf(
      rows[kNewName], sizeof(rows[kNewName]), "%-15s %s%s", "Name", g_name,
      cursor);
  items[kNewName] = (UIBoxItem){rows[kNewName], false};
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
    items[item] = (UIBoxItem){rows[item], true};
  }
  items[kNewCreate] = (UIBoxItem){g_editing ? "Save" : "Create", false};
  UIPageDrawItems(
      menu, g_editing ? g_vms->vms[g_vm].name : "New VM", NULL, 0,
      &items[FormFirstItem()], (uint8_t)(kNewItemCount - FormFirstItem()),
      g_chosen, true);
}

// The choices for one of the form's drives.
static void DrawNewDrivePage(DisplayMenu* menu) {
  uint8_t count = 0;
  const NewDriveItem* choices = DriveChoices(&count);
  const UIBoxItem* labels =
      g_new_drive >= kVMDriveHD1 ? kNewHardDiskItems : kNewFloppyItems;
  UIBoxItem items[kNewDriveItemCount];
  for (uint8_t row = 0; row < count; ++row) {
    items[row] = labels[choices[row]];
  }
  UIPageDrawItems(
      menu, kVMDriveNames[g_new_drive], NULL, 0, items, count, g_chosen, true);
}

static void DrawFontPage(DisplayMenu* menu) {
  UIBoxItem items[kFont4x10Count];
  for (uint8_t i = 0; i < kFont4x10Count; ++i) {
    items[i] = (UIBoxItem){Font4x10Label((Font4x10)i), false};
  }
  UIPageDrawItems(menu, "Font", NULL, 0, items, kFont4x10Count, g_chosen, true);
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
  char what[kUIPageNarrowColumns];
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
  UIPageDrawItems(
      menu, "USB disk mode", lines, sizeof(lines) / sizeof(lines[0]),
      kSharingItems, 1, g_chosen, true);
}

void LauncherMenuDraw(uint64_t now_us) {
  if (g_page == kListPageSharing &&
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
    case kListPageBrowse:
      UIBrowserDraw(&g_browser, menu, g_chosen);
      break;
    case kListPageVMs:
      DrawVMsPage(menu);
      break;
    case kListPageUSB:
      UIPageDrawItems(
          menu, "USB disk mode", NULL, 0, kUSBItems, kUSBItemCount, g_chosen,
          true);
      break;
    case kListPageSharing:
      DrawSharingPage(menu);
      g_status_drawn_us = now_us;
      break;
    case kListPageStopSharing:
      UIPageDrawItems(
          menu, "USB disk mode",
          (const char* const[]){
              "The computer has not ejected the",
              "disk, and may not have finished",
              "writing to it.",
          },
          3, kStopSharingItems, kStopSharingItemCount, g_chosen, true);
      break;
    case kListPageManage:
      DrawManagePage(menu);
      break;
    case kListPageVM:
      DrawVMPage(menu);
      break;
    case kListPageName:
      DrawNamePage(menu);
      break;
    case kListPageDelete:
      DrawDeletePage(menu);
      break;
    case kListPageCopying:
      DrawCopyingPage(menu);
      break;
    case kListPageNew:
      DrawNewPage(menu);
      break;
    case kListPageNewDrive:
      DrawNewDrivePage(menu);
      break;
    case kListPageSettings:
      UIPageDrawItems(
          menu, "Settings", NULL, 0, kSettingsItems, kSettingsItemCount,
          g_chosen, true);
      break;
    case kListPageFont:
      DrawFontPage(menu);
      break;
  }
  // Every page is drawn the list's size, so the box never changes size, and
  // there is never part of a last one left showing to draw over.
  DisplayEndMenu();
  g_dirty = false;
}
