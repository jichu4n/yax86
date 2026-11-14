// The list of VMs on the card, a folder each in `yax86/vm`, and what
// changes the VMs as folders: making a new one, editing its drives, renaming
// it, copying it as a new VM, and deleting it, for the menu's Manage VMs.
//
// Everything in a VM's folder is its own, and goes with it: renamed, copied
// or deleted. An image a VM names outside its folder is shared, and stays
// where it is - a copy names the same image. What names the VM or a file in
// its folder follows a rename: boot_default, and any VM's drives, the VM's
// own written from the card's root included.
#ifndef YAX86_PICOCALC_VM_LIST_H
#define YAX86_PICOCALC_VM_LIST_H

#include <stdbool.h>
#include <stdint.h>

#include "fs.h"
#include "settings.h"
#include "vm_config.h"

enum {
  // The most VMs listed: the first found past this are left out.
  kVMListMaxNumVMs = 16,
  // The most files and VMs VMListDescribe() names.
  kVMListNumNamed = 3,
};

typedef struct VMList {
  VMEntry vms[kVMListMaxNumVMs];
  uint8_t num_vms;
  // Whether there were more VMs than the list holds.
  bool is_truncated;
} VMList;

// List the VMs in `yax86/vm`, sorted by name. An empty list, with
// no card or no such folder, means there are none to choose from.
void VMListRead(VMList* list);

// The index in the list of the VM named name, in any capitals, or
// kVMListMaxNumVMs if there is none.
uint8_t VMListFind(const VMList* list, const char* name);

// What deleting a VM would take with it.
typedef struct VMListContents {
  // The files and folders in the VM's folder besides its vm.cfg, and the
  // first few of their names.
  uint8_t num_files;
  char files[kVMListNumNamed][kFSMaxName + 1];
  // The other VMs with a drive naming a file in the folder, which would be
  // left without that disk, and the first few of their names.
  uint8_t num_users;
  char users[kVMListNumNamed][kVMNameLength + 1];
} VMListContents;

// What a VM's drives are to hold: each an image, a path from the card's root
// or empty for none, or NULL for floppy disk 2 left out of the machine - or a
// new blank disk of the VM's own.
typedef struct VMListDrives {
  const char* paths[kNumVMDrives];
  bool is_blank[kNumVMDrives];
} VMListDrives;

// How making, renaming or cloning a VM went.
typedef enum VMListResult {
  kVMListDone = 0,
  // What was asked will not do - a name, or one image in two drives - and
  // nothing was changed.
  kVMListRefused,
  // The card would not do it, or did part of it.
  kVMListFailed,
} VMListResult;

// What deleting vm would take, of the VMs in list.
void VMListDescribe(
    const VMList* list, const VMEntry* vm, VMListContents* contents);

// Rename vm to name - its folder - and write what names it or a file in it
// again to follow: boot_default, in settings and on the card, and the drives
// of the VMs in list. renamed is
// the VM under its new name. A name is up to kFSMaxFolderName letters,
// digits, - and _, and no DOS device's. Anything but kVMListDone comes with
// the problem described: a name that will not do, or one taken; or a folder
// that could not be renamed, or something naming it that could not be
// written again after.
VMListResult VMListRename(
    const VMList* list, Settings* settings, const VMEntry* vm, const char* name,
    VMEntry* renamed, char problem[kVMProblemLength + 1]);

// Make a new VM, name: its folder, with a vm.cfg naming its drives, and its
// blank disks, written through transfer's buffer, calling its progress as it
// goes. A blank disk is zeros, unformatted as a new disk was, for DOS's
// FORMAT - and for a hard disk, FDISK first: a 360KB floppy for drive A or B,
// `fd1.img` or `fd2.img`, and the core's 10MB hard disk for hard disk 1 or
// 2, `hd1.img` or `hd2.img` - or `fd1_2.img`, `hd1_2.img` and on, if the name
// is taken.
// One image in two drives is refused. created is the new VM. Anything but
// kVMListDone comes with the problem described, as for VMListRename(); a
// VM that could not be made has what was made of it deleted.
VMListResult VMListCreate(
    const char* name, const VMListDrives* drives, FSTransfer* transfer,
    VMEntry* created, char problem[kVMProblemLength + 1]);

// Change what vm's drives hold, as VMListCreate() makes them. An image of
// the VM's own that a drive no longer holds stays in its folder, deleted with
// the VM. Anything but kVMListDone comes with the problem described; blank
// disks made for a change that could not be saved are deleted.
VMListResult VMListEdit(
    const VMEntry* vm, const VMListDrives* drives, FSTransfer* transfer,
    char problem[kVMProblemLength + 1]);

// Copy vm's folder as a new VM, name, moving the files through transfer's
// buffer and calling its progress as it goes. The copy's drives that name a
// file in the original's folder are written again to name its own. copy is
// the new VM. Anything but kVMListDone comes with the problem described, as
// for VMListRename(); a copy that could not be made has what was made of it
// deleted.
VMListResult VMListClone(
    const VMEntry* vm, const char* name, FSTransfer* transfer, VMEntry* copy,
    char problem[kVMProblemLength + 1]);

// Delete vm's folder and everything in it, its vm.cfg last, so that a delete
// cut short leaves a VM to delete again. boot_default, if it named the VM, is
// cleared in settings and on the card, and the first VM listed is the
// default. Other VMs' drives naming a
// file in it are left as they are, and find it gone when they start.
bool VMListDelete(
    Settings* settings, const VMEntry* vm, char problem[kVMProblemLength + 1]);

#endif  // YAX86_PICOCALC_VM_LIST_H
