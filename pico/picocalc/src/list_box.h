// A list box: a box over the guest's screen that lists items, one of them
// selected, such as the launcher's VMs or the settings. Most screens in both
// modes are one, and the in-VM menu draws its pages with it too.
//
// This has what they have in common, apart from drawing the box itself,
// which is ui_box.h's:
//
// - handling a key: moving the selection with the move keys, and telling
//   the screen whether Enter or Space selected the item, or Esc or Backspace
//   went back;
// - keeping the selected row of a long list in view, with arrows on the
//   border where there is more;
// - drawing the items, under lines of text if there are any;
// - noticing a box of another size than the one last drawn.
//
// A list box's state is its screen's. These functions take it, the top row
// of a list and the box last drawn as parameters, and keep nothing of their
// own.
#ifndef YAX86_PICOCALC_LIST_BOX_H
#define YAX86_PICOCALC_LIST_BOX_H

#include <stdbool.h>
#include <stdint.h>

#include "display.h"
#include "southbridge.h"
#include "ui_box.h"

enum {
  // The width of a box, in columns. Most boxes are narrow, centered on the
  // guest's screen. The in-VM menu's status page takes the screen's whole
  // width.
  kListBoxNarrowColumns = 40,
  kListBoxWideColumns = kTextColumns,
  // The rows of a list, which with its edges fill the most rows a box can
  // have. Page Up and Page Down move this far.
  kListBoxRows = kDisplayMenuMaxRows - 2,
};

// The box last drawn. A new box of another size would leave part of the old
// one showing, so the guest's screen is drawn under it first.
typedef struct ListBoxFrame {
  // The box's size, in columns and rows.
  uint8_t columns;
  uint8_t rows;
  // Whether a box is showing: false once the guest's screen has been drawn
  // over it.
  bool drawn;
} ListBoxFrame;

// A list box's state.
typedef struct ListBox {
  // The index of the item selected.
  uint8_t selected;
  // Whether what is on the panel is out of date, so that the box is to be
  // drawn again.
  bool dirty;
} ListBox;

// What a key did in a list box, which the screen acts on.
typedef enum ListBoxAction {
  // Nothing for the screen to do: a key released, or one a list box has no
  // use for.
  kListBoxActionNone = 0,
  // A move key, which moved the selection if there were items to move
  // among. A long list keeps the selected row in view.
  kListBoxActionMoved,
  // Enter or Space: the selected item is to be carried out.
  kListBoxActionSelect,
  // Esc or Backspace: go back.
  kListBoxActionBack,
} ListBoxAction;

// The keys of most list boxes, for the status rows.
extern const char kListBoxHint[];

// Whether event is a key pressed, or a held key repeating, rather than a key
// released. A screen acts only on these. A press also clears the problem the
// status rows show, as any key does. A screen with keys of its own checks
// this before handling them, and hands the rest to ListBoxHandleKeyCode().
bool ListBoxIsKeyPress(SouthbridgeKeyEvent event);

// Handle a key event in a list box of count items. A release does nothing.
// A press is handled as ListBoxHandleKeyCode() handles its key code.
ListBoxAction ListBoxHandleKeyEvent(
    ListBox* list_box, SouthbridgeKeyEvent event, uint8_t count);

// Handle the code of a key pressed in a list box of count items. A move key
// moves the selection, and Enter, Space, Esc and Backspace say what the
// screen is to do.
ListBoxAction ListBoxHandleKeyCode(
    ListBox* list_box, uint8_t key, uint8_t count);

// Start drawing a list box that is out of date. Returns the box to draw it
// in, or NULL if it is not out of date or core 1 is still drawing the last
// one. ListBoxEndDraw() hands the box to core 1.
DisplayMenu* ListBoxBeginDraw(ListBox* list_box);
void ListBoxEndDraw(ListBox* list_box);

// Whether key moves the selection: Up, Down, Page Up or Page Down.
bool ListBoxIsMoveKey(uint8_t key);

// Move *selected for a move key, among count items. Up and Down wrap
// around, and Page Up and Page Down stop at the ends. Returns whether there
// are items to move among, which is when the box needs drawing again.
bool ListBoxMove(uint8_t key, uint8_t count, uint8_t* selected);

// Keep row among a list's rows, with *top the row at the top of them.
void ListBoxScrollTo(uint8_t row, uint8_t* top);

// Draw up and down arrows on a list's border where it has more rows than it
// shows, above or below. The list has total rows, and shows them from top.
void ListBoxDrawScrollArrows(DisplayMenu* menu, uint8_t top, uint8_t total);

// Draw count items, under line_count lines of text if there are any, with
// the item at selected marked. With list_sized, the box is at least the size
// of a list, and has no blank row above the items when there are no lines:
// at power on there is no guest's screen to draw under a smaller box to hide
// the list's.
void ListBoxDrawItems(
    DisplayMenu* menu, const char* title, const char* const* lines,
    uint8_t line_count, const UIBoxItem* items, uint8_t count, uint8_t selected,
    bool list_sized);

// Whether menu is a box of another size than the one last drawn, so that the
// guest's screen has to be drawn under it first.
bool ListBoxFrameResized(const ListBoxFrame* frame, const DisplayMenu* menu);

// Record menu as the box last drawn.
void ListBoxFrameDrawn(ListBoxFrame* frame, const DisplayMenu* menu);

#endif  // YAX86_PICOCALC_LIST_BOX_H
