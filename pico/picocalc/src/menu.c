#include "menu.h"

#include <stddef.h>
#include <stdio.h>

#include "display.h"
#include "file_list.h"

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
  // Where an entry's note starts on its row: a folder's <DIR>, and the image
  // in the drive's mark.
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
  kMenuPageDriveA,
  kMenuPageBrowse,
} MenuPage;

static const char* const kPageNames[] = {
    [kMenuPageMain] = "main",     [kMenuPageStatus] = "status",
    [kMenuPageReset] = "reset",   [kMenuPageDriveA] = "drive_a",
    [kMenuPageBrowse] = "browse",
};

// An item of a page, and whether choosing it opens another menu, which a
// chevron at its end says.
typedef struct MenuItem {
  const char* text;
  bool opens_menu;
} MenuItem;

typedef enum MainItem {
  kMainResume = 0,
  kMainDriveA,
  kMainStatus,
  kMainReset,
  kMainItemCount,
} MainItem;

static const MenuItem kMainItems[kMainItemCount] = {
    [kMainResume] = {"Resume", false},
    [kMainDriveA] = {"Drive A", true},
    [kMainStatus] = {"Status", false},
    [kMainReset] = {"Reset the machine", false},
};

typedef enum DriveItem {
  kDriveInsert = 0,
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

static MenuConfig g_config;
static bool g_open;
static MenuPage g_page;
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
// Which Shift keys are held, from the controller's events.
static bool g_left_shift_held;
static bool g_right_shift_held;

void MenuInit(const MenuConfig* config) { g_config = *config; }

bool MenuIsOpen(void) { return g_open; }

const char* MenuPageName(void) { return kPageNames[g_page]; }

const char* MenuHint(void) {
  if (g_page == kMenuPageStatus) {
    return "Esc back";
  }
  if (g_page == kMenuPageBrowse) {
    return "\x18\x19 move   Enter open   Backspace up a folder   Esc back";
  }
  return "\x18\x19 move   Enter choose   Esc back";
}

static void ScrollToChosen(void);

static char Upper(char c) {
  return (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
}

// Whether two paths name the same file. The card's file system ignores case.
static bool SamePath(const char* a, const char* b) {
  while (*a != '\0' && Upper(*a) == Upper(*b)) {
    ++a;
    ++b;
  }
  return *a == '\0' && *b == '\0';
}

// Show the browser on a folder, with the image in drive A chosen if it is
// there.
static void Browse(const char* path) {
  FileListRead(&g_list, path, kFloppyImageSize);
  g_page = kMenuPageBrowse;
  g_chosen = 0;
  g_list_top = 0;
  g_dirty = true;
  const DiskImage* image = g_config.drive_a();
  if (image->status != kDiskImageOpen) {
    return;
  }
  char path_of_entry[kFileListMaxPath + 1];
  for (uint8_t i = 0; i < g_list.count; ++i) {
    if (g_list.entries[i].kind == kFileListImage &&
        FileListEntryPath(&g_list, &g_list.entries[i], path_of_entry) &&
        SamePath(path_of_entry, image->path)) {
      g_chosen = i;
      ScrollToChosen();
    }
  }
}

// The folder the browser starts in: drive A's image's, or yax86.
static void StartBrowsing(void) {
  const DiskImage* image = g_config.drive_a();
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
        SamePath(path_of_entry, left)) {
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

static uint8_t PageItemCount(void) {
  switch (g_page) {
    case kMenuPageMain:
      return kMainItemCount;
    case kMenuPageReset:
      return kResetItemCount;
    case kMenuPageDriveA:
      return kDriveItemCount;
    case kMenuPageBrowse:
      return g_list.count;
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
      ShowPage(kMenuPageDriveA, kDriveInsert);
      break;
    case kMenuPageReset:
      ShowPage(kMenuPageMain, kMainReset);
      break;
    case kMenuPageDriveA:
      ShowPage(kMenuPageMain, kMainDriveA);
      break;
    case kMenuPageStatus:
    case kMenuPageMain:
      ShowPage(kMenuPageMain, kMainStatus);
      break;
  }
  return kMenuActionNone;
}

// Open the chosen entry of the browser: a folder to look in, or an image to
// put in drive A, which closes the menu.
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
  g_config.insert_drive_a(path);
  g_open = false;
  return kMenuActionClosed;
}

static MenuAction Choose(void) {
  switch (g_page) {
    case kMenuPageMain:
      switch ((MainItem)g_chosen) {
        case kMainResume:
          g_open = false;
          return kMenuActionClosed;
        case kMainDriveA:
          ShowPage(kMenuPageDriveA, kDriveInsert);
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
    case kMenuPageDriveA:
      if (g_chosen == kDriveInsert) {
        StartBrowsing();
      } else {
        g_config.eject_drive_a();
        g_dirty = true;
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
  if (TrackHotkey(event)) {
    g_open = !g_open;
    if (g_open) {
      g_box_drawn = false;
      ShowPage(kMenuPageMain, kMainResume);
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
  // blank row under them.
  PlaceBox(menu, kNarrowColumns, (uint8_t)(count + 4 + text_rows));
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
  BlankRow(&menu->text[row++], menu->columns);
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
  const DiskImage* image = g_config.drive_a();
  char line[kNarrowColumns];
  const char* in_drive = "the built-in boot floppy";
  char name[kDiskImageNameLength + 1];
  const bool empty = image->status == kDiskImageEjected;
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
  DrawItems(
      menu, "Drive A", lines, sizeof(lines) / sizeof(lines[0]), items,
      kDriveItemCount);
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
  const DiskImage* image = g_config.drive_a();
  for (uint8_t i = 0; i < kBrowseRows; ++i) {
    DisplayTextRow* row = &menu->text[1 + i];
    const uint8_t index = (uint8_t)(g_list_top + i);
    if (index >= g_list.count) {
      BlankRow(row, menu->columns);
      if (i == 0 && !g_list.readable) {
        PutText(row, menu->columns, 3, "The card cannot be read", kAttrText);
      } else if (i == 1 && g_list.count <= 1) {
        PutText(
            row, menu->columns, 3, "No folders or floppy images", kAttrText);
      }
      continue;
    }
    const FileListEntry* entry = &g_list.entries[index];
    ItemRow(row, menu->columns, entry->name, index == g_chosen);
    const uint8_t attr = index == g_chosen ? kAttrChosen : kAttrText;
    char path[kFileListMaxPath + 1];
    if (entry->kind != kFileListImage) {
      PutText(row, menu->columns, kBrowseNoteColumn, "<DIR>", attr);
    } else if (
        image->status == kDiskImageOpen &&
        FileListEntryPath(&g_list, entry, path) &&
        SamePath(path, image->path)) {
      PutText(row, menu->columns, kBrowseNoteColumn, "in the drive", attr);
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
          menu, "Reset",
          (const char* const[]){"Restart the machine from power on?"}, 1,
          kResetItems, kResetItemCount);
      break;
    case kMenuPageDriveA:
      DrawDrivePage(menu);
      break;
    case kMenuPageBrowse:
      DrawBrowsePage(menu);
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
