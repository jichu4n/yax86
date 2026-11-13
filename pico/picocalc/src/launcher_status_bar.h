// The status rows in launcher mode, while the list or a screen reached from
// it shows: the keys of the screen showing, or a problem in their place
// until the next key, and the battery. There is no guest, so there are no
// drives or locks.
#ifndef YAX86_PICOCALC_LAUNCHER_STATUS_BAR_H
#define YAX86_PICOCALC_LAUNCHER_STATUS_BAR_H

// Bring the rows up to date, with hint the keys of the screen showing, or
// NULL: compose them again if what they show has changed, and send them to
// core 1.
void LauncherStatusBarUpdate(const char* hint);

#endif  // YAX86_PICOCALC_LAUNCHER_STATUS_BAR_H
