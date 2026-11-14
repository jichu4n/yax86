#include "question_screen.h"

#include <stddef.h>

#include "box.h"
#include "display.h"
#include "list_box.h"

typedef enum QuestionItem {
  kQuestionCancel = 0,
  kQuestionAction,
  kQuestionItemCount,
} QuestionItem;

extern const Screen kQuestionScreen;

static const Question* g_question;
// Where the answer goes, the parent's.
static bool* g_confirmed;
// The list box: the item selected, and whether it is out of date.
static ListBox g_list_box;

ScreenResult QuestionScreenOpen(const Question* question, bool* confirmed) {
  g_question = question;
  g_confirmed = confirmed;
  *g_confirmed = false;
  g_list_box.selected = kQuestionCancel;
  g_list_box.dirty = true;
  return ScreenOpen(&kQuestionScreen);
}

static ScreenResult OnKey(SouthbridgeKeyEvent event) {
  switch (ListBoxHandleKeyEvent(&g_list_box, event, kQuestionItemCount)) {
    case kListBoxActionSelect:
      *g_confirmed = g_list_box.selected == kQuestionAction;
      return ScreenBack();
    case kListBoxActionBack:
      return ScreenBack();
    default:
      return ScreenStays();
  }
}

static bool OnDraw(DisplayBox* box, Y_UNUSED uint64_t now_us) {
  if (!ListBoxTakeDirty(&g_list_box)) {
    return false;
  }
  const BoxItem items[kQuestionItemCount] = {
      [kQuestionCancel] = {"Cancel", false},
      [kQuestionAction] = {g_question->action, false},
  };
  ListBoxDrawItems(
      box, g_question->title, &g_question->question, 1, items,
      kQuestionItemCount, g_list_box.selected, false);
  return true;
}

// Nothing opens over the question.
static ScreenResult OnResume(void) {
  g_list_box.dirty = true;
  return ScreenStays();
}

static const char* Hint(void) { return kListBoxHint; }

static const char* PageName(void) { return g_question->page_name; }

const Screen kQuestionScreen = {
    .on_key = OnKey,
    .on_draw = OnDraw,
    .on_resume = OnResume,
    .hint = Hint,
    .page_name = PageName,
};
