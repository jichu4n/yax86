// A floppy drive's page in the in-VM menu: what the drive holds, and two
// items. "Choose disk image", or "Swap disk image" while the drive has a
// disk, opens the browser to find a floppy image on the card. "Eject", shown
// only while the drive has a disk, takes it out.
//
// An image selected in the browser goes in the drive, and the whole menu
// closes, so that the VM carries on with it. Eject leaves the page showing,
// with the first item selected. Either way the page says so on the status
// rows, and saves what the drive now holds to the VM's vm.cfg, so that the
// next start has it.
#ifndef YAX86_PICOCALC_FLOPPY_DRIVE_SCREEN_H
#define YAX86_PICOCALC_FLOPPY_DRIVE_SCREEN_H

#include "emulator_session.h"
#include "floppy.h"
#include "screen.h"

// Show drive's page, changing the disk in session's drive. session must
// outlive the screen.
ScreenResult FloppyDriveScreenOpen(EmulatorSession* session, FloppyDrive drive);

#endif  // YAX86_PICOCALC_FLOPPY_DRIVE_SCREEN_H
