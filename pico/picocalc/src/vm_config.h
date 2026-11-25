// A VM: a folder in `yax86/vm`, holding a configuration file, `vm.cfg`,
// that names the images in the VM's drives, its video adapter and its memory
// size, and whichever of those images are its own. A VM is named for its
// folder - `yax86/vm/DOS330` is the VM DOS330. The list of them is in
// vm_list.h, and the card's own settings in settings.h.
//
//   fd1=../../images/msdos330/disk01.img
//   fd1_type=360k
//   hd1=hd1.img
//   hd2=/shared/tools.img
//   video_adapter=mda
//   memory_kb=640
//   speed=1x
//
// Each line is logfmt - any number of key=value pairs - and a line starting
// with # is a comment. A path is relative to the folder the file is in, or
// to the card's root with a leading /. Floppy disk 1 not named has no disk
// in it. Floppy disk 2 not named is not there - the BIOS reports one floppy
// drive, and DOS's B: is drive A by another name - and named as fd2="" it is
// a drive with no disk. A hard disk not named, or named as "", is not there:
// a hard disk with no image is no disk at all. Floppy images are 360KB,
// 720KB, 1.2MB or 1.44MB, checked against fd1_type and fd2_type (360k, 720k,
// 1.2m or 1.44m, and 360k when not named), and hard disk images the core's
// 10MB drive. video_adapter is cga or mda, and cga when not named. memory_kb
// is a multiple of 16 from 64 to 640, and 640 when not named. speed is
// 1x or max, and 1x when not named. Keys this firmware does not
// know are skipped.
#ifndef YAX86_PICOCALC_VM_CONFIG_H
#define YAX86_PICOCALC_VM_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#include "config_file.h"
#include "core/fdc.h"
#include "core/platform.h"
#include "core/video.h"
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
  kNumVMDrives,
  kNumVMFloppyDrives = 2,

  // Conventional memory in KB: 64 to 640 in 16KB steps, the granularity
  // GLaBIOS probes at, and 640 when not named.
  kVMMinMemoryKB = Y_PLATFORM_MIN_CONVENTIONAL_MEMORY_SIZE / 1024,
  kVMMaxMemoryKB = Y_PLATFORM_MAX_CONVENTIONAL_MEMORY_SIZE / 1024,
  kVMMemoryStepKB = 16,
  kVMDefaultMemoryKB = kVMMaxMemoryKB,

  // A VM's name: its folder's, an 8.3 name.
  kVMNameLength = 12,
  // The longest problem described.
  kVMProblemLength = kConfigFileProblemLength,
};

// The folder the VMs are in, from the card's root.
extern const char kVMsFolder[];

// Each drive's label - FD1, FD2, HD1, HD2 - and name - Floppy disk 1 ...
// Hard disk 2 -
// as the menu and status rows show them.
extern const char* const kVMDriveLabels[kNumVMDrives];
extern const char* const kVMDriveNames[kNumVMDrives];

// A VM: its name, in capitals, and its file's path from the card's root,
// `yax86/vm/NAME/vm.cfg`.
typedef struct VMEntry {
  char name[kVMNameLength + 1];
  char path[kFSMaxPath + 1];
} VMEntry;

// Speed mode for a VM: 4.77MHz (a real PC/XT, the default) or Max (runs the
// CPU as fast as the host allows while keeping guest time locked to real time).
typedef enum VMSpeed {
  kVMSpeed1x = 0,
  kVMSpeedMax,
  kNumVMSpeeds,
} VMSpeed;

// A VM's machine settings, outside its drives: video adapter, conventional
// memory in KB, and speed mode.
typedef struct VMMachine {
  YVideoAdapter video_adapter;
  uint16_t memory_kb;
  VMSpeed speed;
} VMMachine;

typedef struct VMConfig {
  VMEntry entry;
  // Each drive's image, as a path from the card's root, or empty.
  char drives[kNumVMDrives][kFSMaxPath + 1];
  // Whether the file names each drive, even as empty. Floppy disk 2 is there
  // only if it does; a hard disk not named is empty either way.
  bool is_present[kNumVMDrives];
  // Each floppy drive's mechanism type: Y_FDC_DRIVE_TYPE_360KB when not named
  // in the file.
  YFDCDriveType floppy_types[kNumVMFloppyDrives];
  // The VM's video adapter: Y_VIDEO_ADAPTER_CGA when not named.
  YVideoAdapter video_adapter;
  // The VM's conventional memory in KB: kVMDefaultMemoryKB when not named.
  uint16_t memory_kb;
  // The VM's speed mode: kVMSpeed1x when not named.
  VMSpeed speed;
} VMConfig;

// A floppy drive type's name in vm.cfg and the report (360k, 720k, 1.2m,
// 1.44m), and its label in the menu and on the status page (360KB, 720KB,
// 1.2MB, 1.44MB).
const char* VMConfigFloppyDriveTypeName(YFDCDriveType type);
const char* VMConfigFloppyDriveTypeLabel(YFDCDriveType type);

// The video adapter's name in vm.cfg and the report (cga, mda), and its label
// in the menu and on the status page (CGA, MDA).
const char* VMConfigVideoAdapterName(YVideoAdapter adapter);
const char* VMConfigVideoAdapterLabel(YVideoAdapter adapter);

// The speed mode's name in vm.cfg and the report (1x, max), and its label
// in the menu and on the status page (4.77MHz, Max).
const char* VMConfigSpeedName(VMSpeed speed);
const char* VMConfigSpeedLabel(VMSpeed speed);

// The entry of the VM named name, whether or not it is on the card. Returns
// false if the name is too long for one.
bool VMEntryMake(const char* name, VMEntry* entry);

// Read a VM's file. Keys this firmware does not know are logged and skipped.
// Returns false, with the problem described in problem, if the file could not
// be read or has a line or value that is not understood; config then holds
// what was understood before it.
bool VMConfigRead(
    const VMEntry* vm, VMConfig* config, char problem[kVMProblemLength + 1]);

// Write a VM's configuration to its file, as VMConfigSaveDrive() writes one
// drive, making the file if there is none. Each drive is a path from the
// card's root, empty for none, or NULL for a drive not there, whose line is
// removed; each present floppy drive's type is written (as 360k, 720k, 1.2m or
// 1.44m, and removed when floppy disk 2 is not there); and video_adapter, as
// cga or mda, and memory_kb are always written. Returns false, with the
// problem described, if a path would be too long or the file could not be
// written.
bool VMConfigSave(
    const VMEntry* vm, const char* const drives[kNumVMDrives],
    const YFDCDriveType floppy_types[kNumVMFloppyDrives],
    const VMMachine* machine, char problem[kVMProblemLength + 1]);

// Save the image in a VM's drive, a path from the card's root or NULL or
// empty for none, to its `vm.cfg`, through ConfigFileUpdate(), written as
// FSRelativePath() makes it from the file's folder. Returns false, with
// the problem described, if the file could not be read, would be too long, or
// could not be written.
bool VMConfigSaveDrive(
    const VMConfig* config, uint8_t drive, const char* path,
    char problem[kVMProblemLength + 1]);

#endif  // YAX86_PICOCALC_VM_CONFIG_H
