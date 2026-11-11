// A VM's folder on the card, as a whole: making a new one, renaming it,
// copying it as a new VM, and deleting it, for the menu's Manage VMs.
//
// Everything in a VM's folder is its own, and goes with it: renamed, copied
// or deleted. An image a VM names outside its folder is shared, and stays
// where it is - a copy names the same image. What names the VM or a file in
// its folder follows a rename: boot_default, and any VM's drives, the VM's
// own written from the card's root included.
#ifndef YAX86_PICOCALC_VM_FOLDER_H
#define YAX86_PICOCALC_VM_FOLDER_H

#include <stdbool.h>
#include <stdint.h>

#include "fs.h"
#include "vm_config.h"

enum {
  // The most files and VMs VMFolderDescribe() names.
  kVMFolderNamed = 3,
};

// What deleting a VM would take with it.
typedef struct VMFolderContents {
  // The files and folders in the VM's folder besides its vm.cfg, and the
  // first few of their names.
  uint8_t file_count;
  char files[kVMFolderNamed][kFSMaxName + 1];
  // The other VMs with a drive naming a file in the folder, which would be
  // left without that disk, and the first few of their names.
  uint8_t user_count;
  char users[kVMFolderNamed][kVMNameLength + 1];
} VMFolderContents;

// What a VM's drives are to hold: each an image, a path from the card's root
// or empty for none - or a new blank disk of the VM's own.
typedef struct VMFolderDrives {
  const char* paths[kVMDrives];
  bool blank[kVMDrives];
} VMFolderDrives;

// How making, renaming or cloning a VM went.
typedef enum VMFolderResult {
  kVMFolderDone = 0,
  // What was asked will not do - a name, or one image in two drives - and
  // nothing was changed.
  kVMFolderRefused,
  // The card would not do it, or did part of it.
  kVMFolderFailed,
} VMFolderResult;

// What deleting vm would take, of the VMs in list.
void VMFolderDescribe(
    const VMList* list, const VMEntry* vm, VMFolderContents* contents);

// Rename vm to name - its folder - and write what names it or a file in it
// again to follow: boot_default and the drives of the VMs in list. renamed is
// the VM under its new name. A name is up to kFSMaxFolderName letters,
// digits, - and _, and no DOS device's. Anything but kVMFolderDone comes with
// the problem described: a name that will not do, or one taken; or a folder
// that could not be renamed, or something naming it that could not be
// written again after.
VMFolderResult VMFolderRename(
    const VMList* list, const VMEntry* vm, const char* name, VMEntry* renamed,
    char problem[kVMProblemLength + 1]);

// Make a new VM, name: its folder, with a vm.cfg naming its drives, and its
// blank disks, written through transfer's buffer, calling its progress as it
// goes. A blank disk is zeros, unformatted as a new disk was, for DOS's
// FORMAT - and for a hard disk, FDISK first: a 360KB floppy for drive A or B,
// `fd1.img` or `fd2.img`, and the core's 10MB hard disk for hard disk 1 or
// 2, `hd1.img` or `hd2.img` - or `fd1_2.img`, `hd1_2.img` and on, if the name
// is taken.
// One image in two drives is refused. created is the new VM. Anything but
// kVMFolderDone comes with the problem described, as for VMFolderRename(); a
// VM that could not be made has what was made of it deleted.
VMFolderResult VMFolderCreate(
    const char* name, const VMFolderDrives* drives, FSTransfer* transfer,
    VMEntry* created, char problem[kVMProblemLength + 1]);

// Change what vm's drives hold, as VMFolderCreate() makes them. An image of
// the VM's own that a drive no longer holds stays in its folder, deleted with
// the VM. Anything but kVMFolderDone comes with the problem described; blank
// disks made for a change that could not be saved are deleted.
VMFolderResult VMFolderEdit(
    const VMEntry* vm, const VMFolderDrives* drives, FSTransfer* transfer,
    char problem[kVMProblemLength + 1]);

// Copy vm's folder as a new VM, name, moving the files through transfer's
// buffer and calling its progress as it goes. The copy's drives that name a
// file in the original's folder are written again to name its own. copy is
// the new VM. Anything but kVMFolderDone comes with the problem described, as
// for VMFolderRename(); a copy that could not be made has what was made of it
// deleted.
VMFolderResult VMFolderClone(
    const VMEntry* vm, const char* name, FSTransfer* transfer, VMEntry* copy,
    char problem[kVMProblemLength + 1]);

// Delete vm's folder and everything in it, its vm.cfg last, so that a delete
// cut short leaves a VM to delete again. boot_default, if it named the VM, is
// cleared, and the first VM listed is the default. Other VMs' drives naming a
// file in it are left as they are, and find it gone when they start.
bool VMFolderDelete(const VMEntry* vm, char problem[kVMProblemLength + 1]);

#endif  // YAX86_PICOCALC_VM_FOLDER_H
