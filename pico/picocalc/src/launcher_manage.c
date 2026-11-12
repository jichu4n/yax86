#include "launcher_manage.h"

#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "disk_image.h"
#include "display.h"
#include "fs.h"
#include "launcher.h"
#include "pico/time.h"
#include "runtime_log.h"
#include "ui_box.h"
#include "ui_browser.h"
#include "ui_page.h"
#include "vm_folder.h"

enum {
  // The part of the guest's RAM a clone's files are copied through, before a
  // VM starts and the RAM is the guest's: 64 of the card's blocks, which go to
  // and from it a run at a time.
  kCopyBufferSize = 32 * 1024,

  kBytesPerKB = 1024,
  kMicrosecondsPerMillisecond = 1000,
};

typedef enum ManagePage {
  // Manage VMs, from the list: the VMs in yax86/vm, the page of the one in
  // g_vm, the name typed to rename or clone it as, the question before
  // deleting it, and a clone being copied.
  kManagePageManage = 0,
  kManagePageVM,
  kManagePageName,
  kManagePageDelete,
  kManagePageCopying,
  // New VM, from Manage VMs: its form, and the choice for one of its drives,
  // g_new_drive.
  kManagePageNew,
  kManagePageNewDrive,
  // The browser, finding an image for a new VM's drive.
  kManagePageBrowse,
} ManagePage;

static const char* const kPageNames[] = {
    [kManagePageManage] = "manage",         [kManagePageVM] = "vm",
    [kManagePageName] = "vm_name",          [kManagePageDelete] = "vm_delete",
    [kManagePageCopying] = "vm_copying",    [kManagePageNew] = "new_vm",
    [kManagePageNewDrive] = "new_vm_drive", [kManagePageBrowse] = "browse",
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

static ManagePage g_page;
static uint8_t g_chosen;
// Whether what is on the panel is out of date.
static bool g_dirty;
// The row at the top of Manage VMs.
static uint8_t g_list_top;
// The VMs on the card, which the launcher keeps and reads afresh after a VM
// is changed.
static const VMList* g_vms;
// Guest RAM, lent before a VM starts, which copies go through.
static uint8_t* g_copy_buffer;
// The browser, finding an image for a new VM's drive.
static UIBrowser g_browser;
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

void LauncherManageInit(const VMList* vms, uint8_t* copy_buffer) {
  g_vms = vms;
  g_copy_buffer = copy_buffer;
}

const char* LauncherManagePageName(void) { return kPageNames[g_page]; }

static void ShowPage(ManagePage page, uint8_t chosen);
static LauncherManageResult Choose(void);
static LauncherManageResult Back(void);
static void ShowVM(uint8_t index);
static uint8_t FormFirstItem(void);
static uint8_t FormItem(void);
static const NewDriveItem* DriveChoices(uint8_t* count);
static uint8_t DriveChoiceRow(NewDriveItem choice);
static void ScrollToChosen(void);
static void RenameVM(void);
static void CloneVM(void);
static void CreateVM(void);
static void EditVM(void);
static void DeleteVM(void);

const char* LauncherManageHint(void) {
  if (g_page == kManagePageName) {
    return g_cloning ? "Enter=Clone  Backspace=Erase  Esc=Cancel"
                     : "Enter=Rename  Backspace=Erase  Esc=Cancel";
  }
  if (g_page == kManagePageCopying) {
    return "";
  }
  if (g_page == kManagePageNew) {
    return FormItem() == kNewName
               ? "Type the name  \x18\x19=Move  Backspace=Erase  Esc=Cancel"
               : "\x18\x19=Move  Enter=Select  Esc=Cancel";
  }
  if (g_page == kManagePageBrowse) {
    return "\x18\x19=Move  Enter=Open  Backspace=Up a folder  Esc=Back";
  }
  return "\x18\x19=Move  Enter=Select  Esc=Back";
}

// Show the browser on a folder, with the image in the drive chosen if it is
// there.
static void Browse(const char* folder) {
  ShowPage(kManagePageBrowse, UIBrowserOpen(&g_browser, folder));
}

// Set the browser up to find an image for a new VM's drive: a hard disk for
// its hard disks and a floppy for its floppy drives, with the one already
// chosen for the drive marked.
static void StartBrowser(void) {
  g_browser.images =
      g_new_drive >= kVMDriveHD1 ? kUIBrowserHardDisks : kUIBrowserFloppies;
  g_browser.current = g_new_drives[g_new_drive];
  for (uint8_t drive = 0; drive < kFloppyDrives; ++drive) {
    g_browser.in_drives[drive] = NULL;
  }
}

// Keep the chosen entry among the browser's rows, or Manage VMs'.
static void ScrollToChosen(void) {
  if (g_page == kManagePageBrowse) {
    UIBrowserScrollTo(&g_browser, g_chosen);
    return;
  }
  uint8_t row = g_chosen;
  if (g_page == kManagePageManage && g_vms->count > 0 &&
      g_chosen >= g_vms->count) {
    ++row;
  }
  UIPageScrollTo(row, &g_list_top);
}

// Show the browser on the folder above, with the folder just left chosen.
static void BrowseUp(void) {
  ShowPage(kManagePageBrowse, UIBrowserUp(&g_browser));
}

static void ShowPage(ManagePage page, uint8_t chosen) {
  g_page = page;
  g_chosen = chosen;
  g_dirty = true;
}

static uint8_t ManageRowCount(void) {
  return (uint8_t)(g_vms->count + (g_vms->count > 0 ? 2 : 1));
}

static uint8_t PageItemCount(void) {
  switch (g_page) {
    case kManagePageBrowse:
      return UIBrowserCount(&g_browser);
    case kManagePageManage:
      return (uint8_t)(g_vms->count + 1);
    case kManagePageNew:
      return (uint8_t)(kNewItemCount - FormFirstItem());
    case kManagePageNewDrive:
      return DriveChoiceRow(kNewDriveItemCount);
    case kManagePageVM:
      return kVMItemCount;
    case kManagePageDelete:
      return kDeleteItemCount;
    case kManagePageName:
    case kManagePageCopying:
      return 0;
  }
  return 0;
}

// Show Manage VMs - a row for each VM, then New VM - with the item at index
// chosen.
static void ShowManaged(uint8_t index) {
  g_list_top = 0;
  ShowPage(kManagePageManage, index);
  ScrollToChosen();
}

void LauncherManageOpen(void) { ShowManaged(0); }

// Show Manage VMs, once the list has been read afresh, with the VM named
// name chosen, or the first if it is not there - and with show_vm, the VM's
// own page.
static void ShowManagedNamed(const char* name, bool show_vm) {
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
  ShowPage(kManagePageVM, kVMEdit);
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
  ShowPage(kManagePageName, 0);
}

// A key on the name's page: a character a name can have to add, Backspace to
// take the last off, Enter to go ahead, and Esc to go back.
static LauncherManageResult TypeName(uint8_t key) {
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
      return kLauncherManageStays;
    default:
      break;
  }
  if (key < 0x80 && FSIsFolderNameCharacter((char)key) &&
      g_name_length < kFSMaxFolderName) {
    g_name[g_name_length++] = (char)toupper(key);
    g_name[g_name_length] = '\0';
    g_dirty = true;
  }
  return kLauncherManageStays;
}

// Show how far a clone has been copied, or a new VM's blank hard disk
// written, in KB: the first call shows the page, which takes no keys until
// another is shown.
static void SetCopyProgress(uint32_t copied_kb, uint32_t total_kb) {
  if (g_page != kManagePageCopying) {
    g_creating = g_page == kManagePageNew;
    ShowPage(kManagePageCopying, 0);
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
  ShowPage(kManagePageNew, (uint8_t)(item - FormFirstItem()));
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
  ShowPage(kManagePageNewDrive, DriveChoiceRow((NewDriveItem)chosen));
}

// Take a choice for a drive on the form, back to the form; or for an image,
// find it in the browser, starting where the one chosen is, if one is.
static void ChooseNewDrive(void) {
  uint8_t count = 0;
  const NewDriveItem choice = DriveChoices(&count)[g_chosen];
  char* path = g_new_drives[g_new_drive];
  if (choice == kNewDriveImage) {
    StartBrowser();
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

// The drives filled in on the form, New VM's or Edit drives'. Its strings
// are the form's, until it is next shown.
static void FormDrives(VMFolderDrives* drives) {
  for (uint8_t drive = 0; drive < kVMDrives; ++drive) {
    drives->paths[drive] = g_new_present[drive] ? g_new_drives[drive] : NULL;
    drives->blank[drive] = g_new_blank[drive];
  }
}

// After Manage VMs has changed the card, read its VMs afresh, and the
// settings, and show Manage VMs again with the VM named name chosen - and
// with show_vm, its page.
static __noinline void ShowManagedVMs(const char* name, bool show_vm) {
  // name may be in the list read over.
  char chosen[kVMNameLength + 1];
  snprintf(chosen, sizeof(chosen), "%s", name);
  LauncherReadCard();
  ShowManagedNamed(chosen, show_vm);
}

// Show why Manage VMs could not do something, and log it.
static void ShowManageProblem(
    const char* event, const VMEntry* vm, const char* problem) {
  LauncherSetProblem(problem);
  SYSTEM_WARN(
      event, Y_LOG_STRING("path", vm->path), Y_LOG_STRING("problem", problem));
}

static __noinline void RenameVM(void) {
  const VMEntry vm = g_vms->vms[g_vm];
  VMEntry renamed;
  char problem[kVMProblemLength + 1];
  const VMFolderResult result =
      VMFolderRename(g_vms, &vm, g_name, &renamed, problem);
  if (result == kVMFolderRefused) {
    LauncherSetProblem(problem);
    return;
  }
  ShowManagedVMs(result == kVMFolderDone ? renamed.name : vm.name, false);
  if (result == kVMFolderDone) {
    SYSTEM_INFO(
        "vm_renamed", Y_LOG_STRING("from", vm.name),
        Y_LOG_STRING("to", renamed.name));
  } else {
    ShowManageProblem("vm_not_renamed", &vm, problem);
  }
}

// Show how far a clone has come, at once: the main loop, which would draw it,
// is waiting for the clone.
static void ShowCopyProgress(uint64_t done, uint64_t total) {
  SetCopyProgress(
      (uint32_t)(done / kBytesPerKB), (uint32_t)(total / kBytesPerKB));
  LauncherUpdateRows();
  LauncherManageDraw();
}

static __noinline void CloneVM(void) {
  const VMEntry vm = g_vms->vms[g_vm];
  VMEntry copy;
  char problem[kVMProblemLength + 1];
  const uint64_t start_us = time_us_64();
  FSTransfer transfer = {
      .buffer = g_copy_buffer,
      .buffer_size = kCopyBufferSize,
      .progress = ShowCopyProgress,
  };
  const VMFolderResult result =
      VMFolderClone(&vm, g_name, &transfer, &copy, problem);
  // Left as the guest would find RAM at power on, whatever it held.
  memset(g_copy_buffer, 0, kCopyBufferSize);
  if (result == kVMFolderRefused) {
    LauncherSetProblem(problem);
    return;
  }
  ShowManagedVMs(result == kVMFolderDone ? copy.name : vm.name, false);
  if (result == kVMFolderDone) {
    SYSTEM_INFO(
        "vm_cloned", Y_LOG_STRING("from", vm.name),
        Y_LOG_STRING("to", copy.name),
        Y_LOG_UINT(
            "ms", (time_us_64() - start_us) / kMicrosecondsPerMillisecond));
  } else {
    ShowManageProblem("vm_not_cloned", &vm, problem);
  }
}

static __noinline void CreateVM(void) {
  VMFolderDrives drives;
  FormDrives(&drives);
  VMEntry created;
  char problem[kVMProblemLength + 1];
  const uint64_t start_us = time_us_64();
  // Blank disks are written through the guest's RAM, which is zeroed after.
  FSTransfer transfer = {
      .buffer = g_copy_buffer,
      .buffer_size = kCopyBufferSize,
      .progress = ShowCopyProgress,
  };
  const VMFolderResult result =
      VMFolderCreate(g_name, &drives, &transfer, &created, problem);
  memset(g_copy_buffer, 0, kCopyBufferSize);
  if (result == kVMFolderRefused) {
    LauncherSetProblem(problem);
    return;
  }
  ShowManagedVMs(created.name, false);
  if (result == kVMFolderDone) {
    SYSTEM_INFO(
        "vm_created", Y_LOG_STRING("name", created.name),
        Y_LOG_BOOL("blank_fd1", drives.blank[kVMDriveFD1]),
        Y_LOG_BOOL("blank_fd2", drives.blank[kVMDriveFD2]),
        Y_LOG_BOOL("blank_hd1", drives.blank[kVMDriveHD1]),
        Y_LOG_BOOL("blank_hd2", drives.blank[kVMDriveHD2]),
        Y_LOG_UINT(
            "ms", (time_us_64() - start_us) / kMicrosecondsPerMillisecond));
  } else {
    LauncherSetProblem(problem);
    SYSTEM_WARN(
        "vm_not_created", Y_LOG_STRING("name", created.name),
        Y_LOG_STRING("problem", problem));
  }
}

static __noinline void EditVM(void) {
  const VMEntry vm = g_vms->vms[g_vm];
  VMFolderDrives drives;
  FormDrives(&drives);
  char problem[kVMProblemLength + 1];
  FSTransfer transfer = {
      .buffer = g_copy_buffer,
      .buffer_size = kCopyBufferSize,
      .progress = ShowCopyProgress,
  };
  const VMFolderResult result = VMFolderEdit(&vm, &drives, &transfer, problem);
  memset(g_copy_buffer, 0, kCopyBufferSize);
  if (result == kVMFolderRefused) {
    LauncherSetProblem(problem);
    return;
  }
  ShowManagedVMs(vm.name, true);
  if (result == kVMFolderDone) {
    SYSTEM_INFO("vm_edited", Y_LOG_STRING("name", vm.name));
  } else {
    ShowManageProblem("vm_not_edited", &vm, problem);
  }
}

static __noinline void DeleteVM(void) {
  const VMEntry vm = g_vms->vms[g_vm];
  char problem[kVMProblemLength + 1];
  const bool deleted = VMFolderDelete(&vm, problem);
  // Gone from the list, it leaves the first VM chosen.
  ShowManagedVMs(vm.name, false);
  if (deleted) {
    SYSTEM_INFO("vm_deleted", Y_LOG_STRING("name", vm.name));
  } else {
    ShowManageProblem("vm_not_deleted", &vm, problem);
  }
}

// Open the chosen entry of the browser: a folder to look in, or an image for
// a new VM's drive.
static LauncherManageResult OpenEntry(void) {
  uint8_t chosen = g_chosen;
  char path[kFSMaxPath + 1];
  uint32_t size = 0;
  switch (UIBrowserChoose(&g_browser, &chosen, path, &size)) {
    case kUIBrowserNothing:
      return kLauncherManageStays;
    case kUIBrowserOpened:
      ShowPage(kManagePageBrowse, chosen);
      return kLauncherManageStays;
    case kUIBrowserChoseImage:
      break;
  }
  FSCopyPath(path, g_new_drives[g_new_drive]);
  g_new_blank[g_new_drive] = false;
  g_new_present[g_new_drive] = true;
  ShowFormItem((uint8_t)(kNewDriveA + g_new_drive));
  return kLauncherManageStays;
}

// Go back a page, or to the list from Manage VMs.
static LauncherManageResult Back(void) {
  switch (g_page) {
    case kManagePageBrowse:
      ShowPage(kManagePageNewDrive, DriveChoiceRow(kNewDriveImage));
      break;
    case kManagePageManage:
      return kLauncherManageLeft;
    case kManagePageVM:
      ShowManaged(g_vm);
      break;
    case kManagePageName:
      ShowPage(kManagePageVM, g_cloning ? kVMClone : kVMRename);
      break;
    case kManagePageDelete:
      ShowPage(kManagePageVM, kVMDelete);
      break;
    case kManagePageCopying:
      // A copy cannot be stopped part way.
      break;
    case kManagePageNew:
      if (g_editing) {
        ShowPage(kManagePageVM, kVMEdit);
      } else {
        ShowManaged(g_vms->count);
      }
      break;
    case kManagePageNewDrive:
      ShowFormItem((uint8_t)(kNewDriveA + g_new_drive));
      break;
  }
  return kLauncherManageStays;
}

static LauncherManageResult Choose(void) {
  switch (g_page) {
    case kManagePageBrowse:
      return OpenEntry();
    case kManagePageManage:
      if (g_chosen == g_vms->count) {
        StartNewVM();
      } else {
        ShowVM(g_chosen);
      }
      return kLauncherManageStays;
    case kManagePageNew:
      if (FormItem() == kNewCreate) {
        if (g_editing) {
          EditVM();
        } else {
          CreateVM();
        }
        return kLauncherManageStays;
      }
      if (FormItem() == kNewName) {
        ShowFormItem(kNewDriveA);
      } else {
        ShowNewDrive((uint8_t)(FormItem() - kNewDriveA));
      }
      return kLauncherManageStays;
    case kManagePageNewDrive:
      ChooseNewDrive();
      return kLauncherManageStays;
    case kManagePageVM:
      if (g_chosen == kVMEdit) {
        StartEditVM();
      } else if (g_chosen == kVMDelete) {
        VMFolderDescribe(g_vms, &g_vms->vms[g_vm], &g_contents);
        ShowPage(kManagePageDelete, kDeleteCancel);
      } else {
        StartNaming(g_chosen == kVMClone);
      }
      return kLauncherManageStays;
    case kManagePageName:
      if (g_cloning) {
        CloneVM();
      } else {
        RenameVM();
      }
      return kLauncherManageStays;
    case kManagePageDelete:
      if (g_chosen == kDeleteConfirm) {
        DeleteVM();
        return kLauncherManageStays;
      }
      return Back();
    case kManagePageCopying:
      return kLauncherManageStays;
  }
  return kLauncherManageStays;
}

LauncherManageResult LauncherManageHandleKey(SouthbridgeKeyEvent event) {
  if (event.state != kSouthbridgeKeyReleased && LauncherClearProblem()) {
    g_dirty = true;
  }
  // A key held down repeats, which moves through the items as a press does.
  if (event.state == kSouthbridgeKeyReleased) {
    return kLauncherManageStays;
  }
  if (g_page == kManagePageName) {
    return TypeName(event.key);
  }
  if (g_page == kManagePageCopying) {
    return kLauncherManageStays;
  }
  if (g_page == kManagePageNew) {
    // Backspace erases the name rather than leaving the form, wherever it
    // is: Esc leaves it.
    if ((FormItem() == kNewName && TypeNewName(event.key)) ||
        event.key == kKeyCodeBackspace) {
      return kLauncherManageStays;
    }
  }
  if (UIPageIsMoveKey(event.key)) {
    if (UIPageMove(event.key, PageItemCount(), &g_chosen)) {
      g_dirty = true;
    }
    ScrollToChosen();
    return kLauncherManageStays;
  }
  switch (event.key) {
    case kKeyCodeEnter:
    case kKeyCodeSpace:
      return Choose();
    case kKeyCodeBackspace:
      // Up a folder in the browser, until the card's root.
      if (g_page == kManagePageBrowse && !UIBrowserAtRoot(&g_browser)) {
        BrowseUp();
        return kLauncherManageStays;
      }
      return Back();
    case kKeyCodeEsc:
      return Back();
    default:
      return kLauncherManageStays;
  }
}

// ============================================================================
// Drawing
// ============================================================================

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
  const uint8_t named = g_contents.file_count < kVMFolderNamed
                            ? g_contents.file_count
                            : kVMFolderNamed;
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

void LauncherManageDraw(void) {
  if (!g_dirty) {
    return;
  }
  DisplayMenu* menu = DisplayBeginMenu();
  if (menu == NULL) {
    return;
  }
  switch (g_page) {
    case kManagePageBrowse:
      UIBrowserDraw(&g_browser, menu, g_chosen);
      break;
    case kManagePageManage:
      DrawManagePage(menu);
      break;
    case kManagePageVM:
      DrawVMPage(menu);
      break;
    case kManagePageName:
      DrawNamePage(menu);
      break;
    case kManagePageDelete:
      DrawDeletePage(menu);
      break;
    case kManagePageCopying:
      DrawCopyingPage(menu);
      break;
    case kManagePageNew:
      DrawNewPage(menu);
      break;
    case kManagePageNewDrive:
      DrawNewDrivePage(menu);
      break;
  }
  // Drawn the list's size, as every page from the list is, so the box never
  // changes size.
  DisplayEndMenu();
  g_dirty = false;
}
