// VMs: a folder each in `yax86/vm`, holding a configuration file,
// `vm.cfg`, that names the images in the VM's drives, and whichever of
// those images are its own. A VM is named for its folder - `yax86/vm/DOS330`
// is the VM DOS330 - and the list at power on is every folder there with a
// `vm.cfg`. There are no VMs anywhere else.
//
//   fd1=../../images/msdos330/disk01.img
//   hd1=hd1.img
//   hd2=/shared/tools.img
//
// Each line is logfmt - any number of key=value pairs - and a line starting
// with # is a comment. A path is relative to the folder the file is in, or
// to the card's root with a leading /. Floppy disk 1 not named has no disk
// in it. Floppy disk 2 not named is not there - the BIOS reports one floppy
// drive, and DOS's B: is drive A by another name - and named as fd2="" it is
// a drive with no disk. A hard disk not named, or named as "", is not there:
// a hard disk with no image is no disk at all. Floppy images are 360KB and
// hard disk images the core's 10MB drive.
//
// The card's own settings are `yax86/yax86.cfg`, in the same form:
//
//   boot_default=DOS330
//   boot_delay=5
//   font=atari800
//
// boot_default is a VM's name. It is the VM the list highlights, and starts
// by itself after boot_delay seconds with no key pressed; 0 starts it at
// once. Without settings, or with a boot_default naming no VM, the first VM
// listed is the default, after 5 seconds. Whichever VM starts becomes
// boot_default for the next power on. font names the 4x10 font.
#ifndef YAX86_PICOCALC_VM_CONFIG_H
#define YAX86_PICOCALC_VM_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "config_file.h"
#include "font_4x10.h"
#include "fs.h"

enum {
  // Floppy disks 1 and 2, the floppy controller's drives 0 and 1, which DOS
  // calls A: and B:; and hard disks 1 and 2, the hard disk controller's
  // master and slave. Not C and D: DOS gives the hard disks'
  // partitions letters - each disk's primary partition, then each one's
  // logical drives - so neither disk is always C: or D:.
  kVMDriveFD1 = 0,
  kVMDriveFD2,
  kVMDriveHD1,
  kVMDriveHD2,
  kVMDrives,

  // The most VMs listed: the first found past this are left out.
  kVMListMax = 16,
  // A VM's name: its folder's, an 8.3 name.
  kVMNameLength = 12,
  // The longest problem described.
  kVMProblemLength = kConfigFileProblemLength,
  // How long the list at power on waits for a key, in seconds, without
  // settings, and at most.
  kVMDefaultBootDelay = 5,
  kVMMaxBootDelay = 60,
};

// Each drive's label - FD1, FD2, HD1, HD2 - and name - Floppy disk 1 ...
// Hard disk 2 -
// as the menu and status rows show them.
extern const char* const kVMDriveLabels[kVMDrives];
extern const char* const kVMDriveNames[kVMDrives];

typedef struct VMSettings {
  // The name of the VM to highlight and start, or empty for the first
  // listed.
  char boot_default[kVMNameLength + 1];
  uint8_t boot_delay;
  Font4x10 font;
} VMSettings;

// A VM: its name, in capitals, and its file's path from the card's root,
// `yax86/vm/NAME/vm.cfg`.
typedef struct VMEntry {
  char name[kVMNameLength + 1];
  char path[kFSMaxPath + 1];
} VMEntry;

typedef struct VMList {
  VMEntry vms[kVMListMax];
  uint8_t count;
  // Whether there were more VMs than the list holds.
  bool truncated;
} VMList;

typedef struct VMConfig {
  VMEntry entry;
  // Each drive's image, as a path from the card's root, or empty.
  char drives[kVMDrives][kFSMaxPath + 1];
  // Whether the file names each drive, even as empty. Floppy disk 2 is there
  // only if it does; a hard disk not named is empty either way.
  bool present[kVMDrives];
} VMConfig;

// List the VMs in `yax86/vm`, sorted by name. An empty list, with
// no card or no such folder, means there are none to choose from.
void VMListRead(VMList* list);

// The index in the list of the VM named name, in any capitals, or
// kVMListMax if there is none.
uint8_t VMListFind(const VMList* list, const char* name);

// The entry of the VM named name, whether or not it is on the card. Returns
// false if the name is too long for one.
bool VMEntryMake(const char* name, VMEntry* entry);

// Read a VM's file. Returns false, with the problem described in problem,
// if the file could not be read or has a line that is not understood; config
// then holds what was understood before it.
bool VMConfigRead(
    const VMEntry* vm, VMConfig* config, char problem[kVMProblemLength + 1]);

// Read the card's settings, which without `yax86/yax86.cfg` are the
// defaults. Returns false, with the problem described, if the file has a line
// that is not understood, settings then holding what was understood before
// it; or if boot_default names no VM, which is then left empty.
bool VMSettingsRead(VMSettings* settings, char problem[kVMProblemLength + 1]);

// Make the VM named name the settings' boot_default, through
// ConfigFileUpdate(); or with name empty, clear it, so that the first VM
// listed is the default. Returns false, with the problem described, if the
// settings could not be read or written.
bool VMSettingsSaveBootDefault(
    const char* name, char problem[kVMProblemLength + 1]);

// Save font as the settings' font, through ConfigFileUpdate(). Returns false,
// with the problem described, if the settings could not be read or written.
bool VMSettingsSaveFont(Font4x10 font, char problem[kVMProblemLength + 1]);

// Write the images in all of a VM's drives - paths from the card's root,
// empty for none, or NULL for a drive not there, whose line is removed - to
// its file, as VMConfigSaveDrive() writes one, making the file if there is
// none. Returns false, with the problem described, if a path
// would be too long or the file could not be written.
bool VMConfigSaveDrives(
    const VMEntry* vm, const char* const drives[kVMDrives],
    char problem[kVMProblemLength + 1]);

// Save the image in a VM's drive, a path from the card's root or NULL or
// empty for none, to its `vm.cfg`, through ConfigFileUpdate(), written as
// FSRelativePath() makes it from the file's folder. Returns false, with
// the problem described, if the file could not be read, would be too long, or
// could not be written.
bool VMConfigSaveDrive(
    const VMConfig* config, uint8_t drive, const char* path,
    char problem[kVMProblemLength + 1]);

#endif  // YAX86_PICOCALC_VM_CONFIG_H
