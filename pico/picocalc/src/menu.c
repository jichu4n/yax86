#include "menu.h"

#include <stddef.h>
#include <stdio.h>

#include "display.h"
#include "file_list.h"
#include "harddisk.h"
#include "usb_disk.h"

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
  // The page of the drive in g_drive.
  kMenuPageDrive,
  kMenuPageBrowse,
  // The VMs to start, at power on, and from there what can go on USB, the
  // card while it is there, and the question before taking it back.
  kMenuPageVMs,
  kMenuPageUSB,
  kMenuPageSharing,
  kMenuPageStopSharing,
} MenuPage;

static const char* const kPageNames[] = {
    [kMenuPageMain] = "main",
    [kMenuPageStatus] = "status",
    [kMenuPageReset] = "reset",
    [kMenuPageDrive] = "drive",
    [kMenuPageBrowse] = "browse",
    [kMenuPageVMs] = "vms",
    [kMenuPageUSB] = "usb",
    [kMenuPageSharing] = "usb_sharing",
    [kMenuPageStopSharing] = "usb_stop",
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
  kMainItemCount,
} MainItem;

static const MenuItem kMainItems[kMainItemCount] = {
    [kMainDriveA] = {"Drive A", true},
    [kMainDriveB] = {"Drive B", true},
    [kMainStatus] = {"Status", true},
    [kMainReset] = {"Reset", false},
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

typedef enum USBItem {
  kUSBCard = 0,
  kUSBImage,
  kUSBItemCount,
} USBItem;

static const MenuItem kUSBItems[kUSBItemCount] = {
    [kUSBCard] = {"Whole SD card", false},
    [kUSBImage] = {"Disk image", true},
};

static const MenuItem kSharingItems[] = {{"Leave", false}};

typedef enum StopSharingItem {
  kStopSharingCancel = 0,
  kStopSharingConfirm,
  kStopSharingItemCount,
} StopSharingItem;

static const MenuItem kStopSharingItems[kStopSharingItemCount] = {
    [kStopSharingCancel] = {"Cancel", false},
    [kStopSharingConfirm] = {"Leave anyway", false},
};

static const MenuItem kUSBRow = {"USB disk", true};
static const MenuItem kBuiltInRow = {"Built-in floppy", false};

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
static char g_usb_image_path[kFileListMaxPath + 1];
static uint32_t g_usb_image_size;
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
  uint8_t i = 0;
  if (problem != NULL) {
    while (problem[i] != '\0' && i < kTextColumns) {
      g_problem[i] = problem[i];
      ++i;
    }
  }
  g_problem[i] = '\0';
}

const char* MenuHint(void) {
  if (g_page == kMenuPageVMs) {
    return "\x18\x19=Move  Enter=Start";
  }
  if (g_page == kMenuPageSharing) {
    return "Enter=Leave";
  }
  if (g_page == kMenuPageStatus) {
    return "Esc=Back";
  }
  if (g_page == kMenuPageBrowse) {
    return "\x18\x19=Move  Enter=Open  Backspace=Up a folder  Esc=Back";
  }
  return "\x18\x19=Move  Enter=Choose  Esc=Back";
}

const char* MenuProblem(void) {
  return g_problem[0] != '\0' ? g_problem : NULL;
}

static char Upper(char c) {
  return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
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
  char path_of_entry[kFileListMaxPath + 1];
  for (uint8_t i = 0; i < g_list.count; ++i) {
    if (g_list.entries[i].kind == kFileListImage &&
        FileListEntryPath(&g_list, &g_list.entries[i], path_of_entry) &&
        FileListSamePath(path_of_entry, image->path)) {
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
  char folder[kFileListMaxPath + 1];
  if (image->status == kDiskImageOpen) {
    FileListFolderOf(image->path, folder);
  } else {
    folder[0] = 'y';
    folder[1] = 'a';
    folder[2] = 'x';
    folder[3] = '8';
    folder[4] = '6';
    folder[5] = '\0';
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
  char left[kFileListMaxPath + 1];
  char parent[kFileListMaxPath + 1];
  FileListCopyPath(g_list.path, left);
  FileListFolderOf(left, parent);
  Browse(parent);
  char path_of_entry[kFileListMaxPath + 1];
  for (uint8_t i = 0; i < g_list.count; ++i) {
    if (g_list.entries[i].kind == kFileListFolder &&
        FileListEntryPath(&g_list, &g_list.entries[i], path_of_entry) &&
        FileListSamePath(path_of_entry, left)) {
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

static uint8_t DriveItemCount(void) {
  return DriveEmpty() ? kDriveEject : kDriveItemCount;
}

// The list at power on's rows: a VM each, the built-in floppy when there are
// none, and USB disk.
static uint8_t VMRowCount(void) {
  return (uint8_t)(g_vms->count + (g_vms->count == 0 ? 1 : 0) + 1);
}

static uint8_t USBRowIndex(void) { return (uint8_t)(VMRowCount() - 1); }

// Whether leaving would take the card from a computer still using it.
static bool ComputerHasCard(void) {
  const UsbDiskStats stats = UsbDiskGetStats();
  return stats.connected && stats.seen && !stats.ejected;
}

static uint8_t PageItemCount(void) {
  switch (g_page) {
    case kMenuPageMain:
      return kMainItemCount;
    case kMenuPageReset:
      return kResetItemCount;
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
    case kMenuPageStatus:
      return 0;
  }
  return 0;
}

// Leave the card's page: at once if no computer is using the card, and after
// asking if one is.
static MenuAction LeaveSharing(void) {
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
      return LeaveSharing();
    case kMenuPageStopSharing:
      ShowPage(kMenuPageSharing, 0);
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
  char path[kFileListMaxPath + 1];
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
    FileListCopyPath(path, g_usb_image_path);
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
        case kMainItemCount:
          break;
      }
      break;
    case kMenuPageReset:
      if (g_chosen == kResetConfirm) {
        g_config.reset();
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
      return kMenuActionVMChosen;
    case kMenuPageUSB:
      if (g_chosen == kUSBImage) {
        g_browsing_for_usb = true;
        Browse("yax86");
        return kMenuActionNone;
      }
      return kMenuActionShareCard;
    case kMenuPageSharing:
      return LeaveSharing();
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
  uint8_t length = 0;
  while (title[length] != '\0') {
    ++length;
  }
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
  char full[kFileListMaxPath + 2];
  full[0] = '/';
  uint16_t length = 1;
  for (const char* c = g_list.path; *c != '\0'; ++c) {
    full[length++] = Upper(*c);
  }
  full[length] = '\0';
  if (length > room) {
    title[0] = '.';
    title[1] = '.';
    for (uint8_t i = 2; i <= room; ++i) {
      title[i] = full[length - room + i];
    }
  } else {
    for (uint16_t i = 0; i <= length; ++i) {
      title[i] = full[i];
    }
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
    char path[kFileListMaxPath + 1];
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
      if (image->status == kDiskImageOpen &&
          FileListSamePath(path, image->path)) {
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
  EdgeRow(&menu->text[0], menu->columns, true, "Start a VM");
  for (uint8_t i = 0; i < kBrowseRows; ++i) {
    DisplayTextRow* row = &menu->text[1 + i];
    const uint8_t index = (uint8_t)(g_list_top + i);
    if (index >= VMRowCount()) {
      BlankRow(row, menu->columns);
    } else if (index == USBRowIndex()) {
      MenuItemRow(row, menu->columns, &kUSBRow, index == g_chosen);
    } else if (index >= g_vms->count) {
      MenuItemRow(row, menu->columns, &kBuiltInRow, index == g_chosen);
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

// The card while it is on USB: who has it, and how much has moved.
static void DrawSharingPage(DisplayMenu* menu) {
  const UsbDiskStats stats = UsbDiskGetStats();
  const char* state = "Waiting for the computer...";
  if (!stats.connected) {
    state = "No computer is connected.";
  } else if (stats.ejected) {
    state = "The computer has ejected it.";
  } else if (stats.seen) {
    state = "The computer has it.";
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
    snprintf(what, sizeof(what), "%s is on USB.", name);
  } else {
    snprintf(what, sizeof(what), "The SD card is on USB.");
  }
  const char* const lines[] = {
      what,
      state,
      moved,
      ComputerHasCard() ? "Eject it there before leaving." : "",
  };
  DrawItems(
      menu, "USB disk", lines, sizeof(lines) / sizeof(lines[0]), kSharingItems,
      1);
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
      DrawItems(menu, "yax86", NULL, 0, kMainItems, kMainItemCount);
      break;
    case kMenuPageReset:
      DrawItems(
          menu, "Reset", (const char* const[]){"Restart the VM from power on?"},
          1, kResetItems, kResetItemCount);
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
      DrawItems(
          menu, "USB disk",
          (const char* const[]){"Put on USB, for a computer:"}, 1, kUSBItems,
          kUSBItemCount);
      break;
    case kMenuPageSharing:
      DrawSharingPage(menu);
      g_status_drawn_us = now_us;
      break;
    case kMenuPageStopSharing:
      DrawItems(
          menu, "USB disk",
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
