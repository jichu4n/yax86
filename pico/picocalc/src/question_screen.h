// A question before doing something that cannot be taken back, such as
// resetting the VM: a box with the question, and two items, Cancel and the
// action. Cancel is selected when it opens.
//
// It only asks. Selecting the action notes the answer and goes back, and the
// screen that opened it carries the action out. Cancel, Esc and Backspace go
// back without noting anything.
#ifndef YAX86_PICOCALC_QUESTION_SCREEN_H
#define YAX86_PICOCALC_QUESTION_SCREEN_H

#include <stdbool.h>

#include "screen.h"

// What a question asks, and how it shows.
typedef struct Question {
  // The box's title, such as "Reset".
  const char* title;
  // The question, such as "Reset the VM?".
  const char* question;
  // The action's item, such as "Reset".
  const char* action;
  // The page's name, as the report gives it, such as "reset".
  const char* page_name;
} Question;

// Ask question. *confirmed is set to false, and to true if the action is
// selected, before the screen goes back. question, its strings and confirmed
// must outlive the screen.
ScreenResult QuestionScreenOpen(const Question* question, bool* confirmed);

#endif  // YAX86_PICOCALC_QUESTION_SCREEN_H
