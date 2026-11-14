#include "menu.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "box.h"
#include "display.h"
#include "file_browser.h"
#include "list_box.h"
#include "vm_config.h"

enum {
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
} MenuPage;

static const char* const kPageNames[] = {
    [kMenuPageMain] = "main",   [kMenuPageStatus] = "status",
    [kMenuPageReset] = "reset", [kMenuPagePowerOff] = "power_off",
    [kMenuPageDrive] = "drive", [kMenuPageBrowse] = "browse",
};

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

static const BoxItem kResetItems[kResetItemCount] = {
    [kResetCancel] = {"Cancel", false},
    [kResetConfirm] = {"Reset", false},
};

typedef enum PowerOffItem {
  kPowerOffCancel = 0,
  kPowerOffConfirm,
  kPowerOffItemCount,
} PowerOffItem;

static const BoxItem kPowerOffItems[kPowerOffItemCount] = {
    [kPowerOffCancel] = {"Cancel", false},
    [kPowerOffConfirm] = {"Power off", false},
};

static MenuConfig g_config;
static bool g_open;
static MenuPage g_page;
// The drive whose page or browser shows.
static FloppyDrive g_drive;
static uint8_t g_selected;
// Whether what is on the panel is out of date, and when the status page was
// last drawn.
static bool g_dirty;
static uint64_t g_status_drawn_us;
// The box last drawn, whose size a new one has to match for nothing of it to
// be left showing.
static BoxShown g_box_shown;
// The browser, finding an image for the drive in g_drive.
static FileBrowser g_browser;
// Which Shift keys are held, from the controller's events.
static bool g_left_shift_held;
static bool g_right_shift_held;

void MenuInit(const MenuConfig* config) { g_config = *config; }

bool MenuIsOpen(void) { return g_open; }

const char* MenuPageName(void) {
  if (g_page == kMenuPageDrive) {
    return g_drive == kFloppyDriveA ? "fd1" : "fd2";
  }
  return kPageNames[g_page];
}

static void ShowPage(MenuPage page, uint8_t selected);
static MenuAction OnSelect(void);
static MenuAction Back(void);

const char* MenuHint(void) {
  if (g_page == kMenuPageStatus) {
    return "Esc=Back";
  }
  if (g_page == kMenuPageBrowse) {
    return "\x18\x19=Move  Enter=Open  Backspace=Up a folder  Esc=Back";
  }
  return "\x18\x19=Move  Enter=Select  Esc=Back";
}

// Show the browser on a folder, with the image in the drive selected if it is
// there.
static void Browse(const char* folder) {
  ShowPage(kMenuPageBrowse, FileBrowserOpen(&g_browser, folder));
}

// The folder the browser starts in: that of the image in the drive, or else
// of the one in the other drive - the next disk of a set is likely beside
// it - or else yax86. The images in the drives are marked, and the one in
// this drive selected.
static void StartBrowsing(void) {
  g_browser.images = kFileBrowserFloppies;
  for (uint8_t drive = 0; drive < kFloppyDrives; ++drive) {
    const DiskImage* image = g_config.drive((FloppyDrive)drive);
    g_browser.in_drives[drive] =
        image->status == kDiskImageOpen ? image->path : NULL;
  }
  g_browser.current = g_browser.in_drives[g_drive];
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

// Keep the selected entry among the browser's rows.
static void ScrollToSelected(void) {
  if (g_page == kMenuPageBrowse) {
    FileBrowserScrollTo(&g_browser, g_selected);
  }
}

// Show the browser on the folder above, with the folder just left selected.
static void BrowseUp(void) {
  ShowPage(kMenuPageBrowse, FileBrowserUp(&g_browser));
}

static void ShowPage(MenuPage page, uint8_t selected) {
  g_page = page;
  g_selected = selected;
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

// The main page's item on the row selected.
static MainItem SelectedMainItem(void) {
  MainItem items[kMainItemCount];
  const uint8_t count = MainItems(items);
  return g_selected < count ? items[g_selected] : kMainItemCount;
}

// Show the main page with an item selected, or the first if it is not shown.
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
      return FileBrowserCount(&g_browser);
    case kMenuPageStatus:
      return 0;
  }
  return 0;
}

// Go back a page, or close the menu from the main one.
static MenuAction Back(void) {
  if (g_page == kMenuPageMain) {
    g_open = false;
    return kMenuActionClosed;
  }
  switch (g_page) {
    case kMenuPageBrowse:
      ShowPage(kMenuPageDrive, kDriveInsert);
      break;
    case kMenuPageReset:
      ShowMainItem(kMainReset);
      break;
    case kMenuPagePowerOff:
      ShowMainItem(kMainPowerOff);
      break;
    case kMenuPageDrive:
      ShowMainItem((MainItem)(kMainDriveA + g_drive));
      break;
    case kMenuPageStatus:
    case kMenuPageMain:
      ShowMainItem(kMainStatus);
      break;
  }
  return kMenuActionNone;
}

// Open the selected entry of the browser: a folder to look in, or an image to
// put in the drive, which closes the menu.
static MenuAction OpenEntry(void) {
  uint8_t selected = g_selected;
  char path[kFSMaxPath + 1];
  uint32_t size = 0;
  switch (FileBrowserSelect(&g_browser, &selected, path, &size)) {
    case kFileBrowserNothing:
      return kMenuActionNone;
    case kFileBrowserOpenedFolder:
      ShowPage(kMenuPageBrowse, selected);
      return kMenuActionNone;
    case kFileBrowserSelectedImage:
      break;
  }
  g_config.insert_drive(g_drive, path);
  g_open = false;
  return kMenuActionClosed;
}

// Carry out the item selected on the page showing.
static MenuAction OnSelect(void) {
  switch (g_page) {
    case kMenuPageMain:
      switch (SelectedMainItem()) {
        case kMainDriveA:
        case kMainDriveB:
          g_drive = (FloppyDrive)(SelectedMainItem() - kMainDriveA);
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
      if (g_selected == kResetConfirm) {
        g_config.reset();
      }
      return Back();
    case kMenuPagePowerOff:
      if (g_selected == kPowerOffConfirm) {
        g_config.power_off();
      }
      return Back();
    case kMenuPageDrive:
      if (g_selected == kDriveInsert) {
        StartBrowsing();
      } else {
        g_config.eject_drive(g_drive);
        // Eject goes with the disk.
        ShowPage(kMenuPageDrive, kDriveInsert);
      }
      return kMenuActionNone;
    case kMenuPageBrowse:
      return OpenEntry();
    case kMenuPageStatus:
      return Back();
  }
  return kMenuActionNone;
}

// Track the Shift keys, and return whether this event completes the hotkey:
// both held, the second just pressed.
static bool TrackHotkey(SouthbridgeKeyEvent event) {
  bool* held = NULL;
  if (event.key == kKeyCodeLeftShift) {
    held = &g_left_shift_held;
  } else if (event.key == kKeyCodeRightShift) {
    held = &g_right_shift_held;
  } else {
    return false;
  }
  const bool was_held = *held;
  *held = event.state != kSouthbridgeKeyReleased;
  return *held && !was_held && g_left_shift_held && g_right_shift_held;
}

MenuAction MenuHandleKey(SouthbridgeKeyEvent event) {
  if (TrackHotkey(event)) {
    g_open = !g_open;
    if (g_open) {
      g_box_shown.drawn = false;
      ShowMainItem(kMainDriveA);
      return kMenuActionOpened;
    }
    return kMenuActionClosed;
  }
  // A key held down repeats, which moves through the items as a press does.
  if (!g_open || event.state == kSouthbridgeKeyReleased) {
    return kMenuActionNone;
  }
  if (ListBoxIsMoveKey(event.key)) {
    if (ListBoxMove(event.key, PageItemCount(), &g_selected)) {
      g_dirty = true;
    }
    ScrollToSelected();
    return kMenuActionNone;
  }
  switch (event.key) {
    case kKeyCodeEnter:
    case kKeyCodeSpace:
      return OnSelect();
    case kKeyCodeBackspace:
      // Up a folder in the browser, until the card's root.
      if (g_page == kMenuPageBrowse && !FileBrowserAtRoot(&g_browser)) {
        BrowseUp();
        return kMenuActionNone;
      }
      return Back();
    case kKeyCodeEsc:
      return Back();
    default:
      return kMenuActionNone;
  }
}

// ============================================================================
// Drawing
// ============================================================================

static void DrawStatusPage(DisplayBox* box) {
  MenuStatusLine lines[kMenuStatusLines];
  g_config.describe_status(lines);
  BoxPlace(box, kListBoxWideColumns, kMenuStatusLines + 4);
  uint8_t row = 0;
  BoxEdgeRow(&box->text[row++], box->columns, true, "Status");
  BoxBlankRow(&box->text[row++], box->columns);
  for (uint8_t i = 0; i < kMenuStatusLines; ++i) {
    DisplayTextRow* text = &box->text[row++];
    BoxBlankRow(text, box->columns);
    BoxPutText(text, box->columns, 2, lines[i].label, kBoxAttrTitle);
    BoxPutText(
        text, box->columns, 2 + kStatusLabelColumns, lines[i].value,
        kBoxAttrText);
  }
  BoxBlankRow(&box->text[row++], box->columns);
  BoxEdgeRow(&box->text[row], box->columns, false, NULL);
}

static void DrawDrivePage(DisplayBox* box) {
  const DiskImage* image = g_config.drive(g_drive);
  char line[kListBoxNarrowColumns];
  const char* in_drive = "the built-in boot floppy";
  char name[kDiskImageNameLength + 1];
  const bool empty = DriveEmpty();
  if (image->status == kDiskImageOpen) {
    DiskImageName(image->path, name);
    in_drive = name;
  } else if (empty) {
    in_drive = "empty";
  }
  const BoxItem items[kDriveItemCount] = {
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
  ListBoxDrawItems(
      box, title, lines, sizeof(lines) / sizeof(lines[0]), items,
      DriveItemCount(), g_selected, false);
}

// The main page: its items as they show.
static void DrawMainPage(DisplayBox* box) {
  MainItem shown[kMainItemCount];
  const uint8_t count = MainItems(shown);
  BoxItem items[kMainItemCount];
  for (uint8_t i = 0; i < count; ++i) {
    items[i] = kMainItems[shown[i]];
  }
  ListBoxDrawItems(box, "yax86", NULL, 0, items, count, g_selected, false);
}

void MenuDraw(uint64_t now_us) {
  if (!g_open) {
    return;
  }
  if (g_page == kMenuPageStatus &&
      now_us - g_status_drawn_us >= kStatusRefreshUs) {
    g_dirty = true;
  }
  if (!g_dirty) {
    return;
  }
  DisplayBox* box = DisplayBeginBox();
  if (box == NULL) {
    return;
  }
  switch (g_page) {
    case kMenuPageMain:
      DrawMainPage(box);
      break;
    case kMenuPageReset:
      ListBoxDrawItems(
          box, "Reset", (const char* const[]){"Reset the VM?"}, 1, kResetItems,
          kResetItemCount, g_selected, false);
      break;
    case kMenuPagePowerOff:
      ListBoxDrawItems(
          box, "Power off", (const char* const[]){"Power off the VM?"}, 1,
          kPowerOffItems, kPowerOffItemCount, g_selected, false);
      break;
    case kMenuPageDrive:
      DrawDrivePage(box);
      break;
    case kMenuPageBrowse:
      FileBrowserDraw(&g_browser, box, g_selected);
      break;
    case kMenuPageStatus:
      DrawStatusPage(box);
      g_status_drawn_us = now_us;
      break;
  }
  // A box of another size than the last leaves part of the last showing, so
  // the guest's screen goes under it first. If core 1 cannot take that yet,
  // the box is laid out again next time.
  if (BoxShownResized(&g_box_shown, box) && !g_config.redraw_guest()) {
    return;
  }
  DisplayEndBox();
  BoxShownRecord(&g_box_shown, box);
  g_dirty = false;
}
