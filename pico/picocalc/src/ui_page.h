// What the menu's pages have in common, apart from the box they are drawn
// in: taking a key, moving the chosen item with the keys, keeping a list's
// chosen entry in view, drawing a page of items, and drawing a box of a new
// size.
//
// A page's state is its user's: these take it, the top of a list and the
// box last drawn as parameters, and keep nothing of their own.
#ifndef YAX86_PICOCALC_UI_PAGE_H
#define YAX86_PICOCALC_UI_PAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "display.h"
#include "southbridge.h"
#include "ui_box.h"

enum {
  // A page's box: the main page and the question are narrow, centered on the
  // guest's screen, and the status page takes its whole width.
  kUIPageNarrowColumns = 40,
  kUIPageWideColumns = kTextColumns,
  // The rows of a list, which with its edges fill the most rows a box can
  // have. Page Up and Page Down move this far.
  kUIPageListRows = kDisplayMenuMaxRows - 2,
};

// The box last drawn, which a new one of another size leaves part of
// showing.
typedef struct UIPageBox {
  uint8_t columns;
  uint8_t rows;
  // Whether there is one: false once the guest's screen has been drawn over
  // it.
  bool drawn;
} UIPageBox;

// A page: the item chosen, and whether what is on the panel is out of date.
typedef struct UIPage {
  uint8_t chosen;
  bool dirty;
} UIPage;

// What a key did on a page.
typedef enum UIPageKey {
  // Nothing for the page to do: a key released, or one a page of items does
  // not take.
  kUIPageKeyNone = 0,
  // A move key, which moved the chosen item if there were items to move
  // among; a list keeps it in view.
  kUIPageKeyMoved,
  // Enter or Space: choose the item chosen.
  kUIPageKeyChoose,
  // Esc or Backspace: go back.
  kUIPageKeyBack,
} UIPageKey;

// The keys of most pages, for the status rows.
extern const char kUIPageHint[];

// Whether event is a press, a held key's repeats included, which a page
// acts on. A press also clears the problem the status rows show, as any key
// does.
bool UIPageKeyPressed(SouthbridgeKeyEvent event);

// Take a key pressed on a page of count items: a move key moves the chosen
// item, and Enter, Space, Esc and Backspace say what the page is to do.
UIPageKey UIPageTakeKey(UIPage* page, uint8_t key, uint8_t count);

// The same for a key event: a release does nothing.
UIPageKey UIPageTakeEvent(
    UIPage* page, SouthbridgeKeyEvent event, uint8_t count);

// Start drawing a page that is out of date: the box to draw it in, or NULL
// if it is not out of date, or core 1 is still drawing the last. Hand the
// box over with UIPageEndDraw().
DisplayMenu* UIPageBeginDraw(UIPage* page);
void UIPageEndDraw(UIPage* page);

// Whether key moves the chosen item: Up, Down, Page Up or Page Down.
bool UIPageIsMoveKey(uint8_t key);

// Move *chosen for a key UIPageIsMoveKey() takes, among count items: Up and
// Down wrap around, and Page Up and Page Down stop at the ends. Returns
// whether there are items to move among, which is when the page needs
// drawing again.
bool UIPageMove(uint8_t key, uint8_t count, uint8_t* chosen);

// Keep row among a list's rows, with *top the row at the top of them.
void UIPageScrollTo(uint8_t row, uint8_t* top);

// Up and down arrows on a list's border where it has more than its rows
// show, above or below: total rows from top.
void UIPageDrawScrollArrows(DisplayMenu* menu, uint8_t top, uint8_t total);

// A page of items, under lines of text, if any, with the item at chosen
// marked. With list_sized, the box is the size of a list at least, and has
// no blank row above the items without lines: at power on there is no
// guest's screen to draw under a smaller box to hide the list's.
void UIPageDrawItems(
    DisplayMenu* menu, const char* title, const char* const* lines,
    uint8_t line_count, const UIBoxItem* items, uint8_t count, uint8_t chosen,
    bool list_sized);

// Whether menu is a box of another size than the one last drawn, so that
// the guest's screen has to be drawn under it first.
bool UIPageBoxResized(const UIPageBox* box, const DisplayMenu* menu);

// Record menu as the box last drawn.
void UIPageBoxDrawn(UIPageBox* box, const DisplayMenu* menu);

#endif  // YAX86_PICOCALC_UI_PAGE_H
