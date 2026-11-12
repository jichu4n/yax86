// The status rows while the list at power on or one of its pages shows:
// the page's keys, or a problem in their place until the next key, and the
// battery. There is no guest, so there are no drives or locks yet.
#ifndef YAX86_PICOCALC_UI_STATUS_H
#define YAX86_PICOCALC_UI_STATUS_H

#include <stdbool.h>

// Show problem in place of the keys until UIStatusClearProblem(); NULL for
// nothing. The text is copied.
void UIStatusSetProblem(const char* problem);

// Stop showing the problem, as a key pressed does. Returns whether there was
// one.
bool UIStatusClearProblem(void);

// Compose the status rows, with hint the keys of the page showing, and hand
// them to core 1 if they changed.
void UIStatusOffer(const char* hint);

#endif  // YAX86_PICOCALC_UI_STATUS_H
