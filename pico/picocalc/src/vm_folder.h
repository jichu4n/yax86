// A VM's folder on the card, as a whole: making a new one, renaming it,
// copying it as a new VM, and deleting it, for the menu's Manage VMs.
//
// Only the VMs in `yax86/vm` are managed - a folder each, named for the VM.
// A VM elsewhere, listed because boot_default names it, is left as it is: its
// folder may be anything, the card's root included.
//
// Everything in a VM's folder is its own, and goes with it: renamed, copied
// or deleted. An image a VM names outside its folder is shared, and stays
// where it is - a copy names the same image. What names a file in a folder
// that moves - boot_default, and any VM's drives, the VM's own written from
// the card's root included - is written again to follow it.
#ifndef YAX86_PICOCALC_VM_FOLDER_H
#define YAX86_PICOCALC_VM_FOLDER_H

#include <stdbool.h>
#include <stdint.h>

#include "vm_config.h"

enum {
  // The longest name a VM can be given: a folder's name, with no extension.
  kVMFolderMaxName = 8,
  // The most files and VMs VMFolderDescribe() names.
  kVMFolderNamed = 3,
};

// What deleting a VM would take with it.
typedef struct VMFolderContents {
  // The files and folders in the VM's folder besides its vm.cfg, and the
  // first few of their names.
  uint8_t file_count;
  char files[kVMFolderNamed][kFileListMaxName + 1];
  // The other VMs with a drive naming a file in the folder, which would be
  // left without that disk, and the first few of their names.
  uint8_t user_count;
  char users[kVMFolderNamed][kVMNameLength + 1];
} VMFolderContents;

// A new VM: its name, and the images in its drives, paths from the card's
// root or empty for none - or for drives A to C, a blank disk of its own.
typedef struct VMFolderNew {
  const char* name;
  const char* drives[kVMDrives];
  bool blank[kVMDriveC + 1];
} VMFolderNew;

// How making, renaming or cloning a VM went.
typedef enum VMFolderResult {
  kVMFolderDone = 0,
  // The name will not do, and nothing was changed.
  kVMFolderBadName,
  // The card would not do it, or did part of it.
  kVMFolderFailed,
} VMFolderResult;

// How a copy is going, in KB, called after each piece is written.
typedef void (*VMFolderProgress)(uint32_t done_kb, uint32_t total_kb);

// Whether a VM is one of the folders in `yax86/vm`, which can be renamed,
// copied and deleted.
bool VMFolderIsManaged(const VMEntry* vm);

// Whether c can be in a VM's name: a letter, a digit, - or _. The card keeps
// names in capitals, so a small letter is taken as its capital.
bool VMFolderNameCharacter(char c);

// What deleting vm would take, of the VMs in list.
void VMFolderDescribe(
    const VMList* list, const VMEntry* vm, VMFolderContents* contents);

// Rename vm's folder to name, in capitals, and write what names a file in it
// again to follow: boot_default and the drives of the VMs in list. path is
// the VM's new vm.cfg. Anything but kVMFolderDone comes with the problem
// described: a name a VM cannot have, or one taken; or a folder that could not
// be renamed, or something naming it that could not be written again after.
VMFolderResult VMFolderRename(
    const VMList* list, const VMEntry* vm, const char* name,
    char path[kFileListMaxPath + 1], char problem[kVMProblemLength + 1]);

// Make a new VM in `yax86/vm`: its folder, with a vm.cfg naming its drives,
// and its blank disks, written through buffer, calling progress as it goes.
// A blank disk is zeros, unformatted, as a new disk was, for DOS's FORMAT -
// and for a hard disk, FDISK first: a 360KB floppy, `floppy_a.img` or
// `floppy_b.img`, or the core's 10MB hard disk, `hdd.img`. path is its vm.cfg. Anything but
// kVMFolderDone comes with the problem described, as for VMFolderRename(); a
// VM that could not be made has what was made of it deleted.
VMFolderResult VMFolderCreate(
    const VMFolderNew* vm, uint8_t* buffer, uint32_t buffer_size,
    VMFolderProgress progress, char path[kFileListMaxPath + 1],
    char problem[kVMProblemLength + 1]);

// Copy vm's folder as a new VM, name, in `yax86/vm`, moving the files through
// buffer, a whole number of the card's blocks long, and calling progress as
// it goes. The copy's drives that name a file in the original's folder are
// written again to name its own. path is the copy's vm.cfg. Anything but
// kVMFolderDone comes with the problem described, as for VMFolderRename(); a
// copy that could not be made has what was made of it deleted.
VMFolderResult VMFolderClone(
    const VMEntry* vm, const char* name, uint8_t* buffer, uint32_t buffer_size,
    VMFolderProgress progress, char path[kFileListMaxPath + 1],
    char problem[kVMProblemLength + 1]);

// Delete vm's folder and everything in it, its vm.cfg last, so that a delete
// cut short leaves a VM to delete again. boot_default, if it named the VM, is
// cleared, and the first VM listed is the default. Other VMs' drives naming a
// file in it are left as they are, and find it gone when they start.
bool VMFolderDelete(const VMEntry* vm, char problem[kVMProblemLength + 1]);

#endif  // YAX86_PICOCALC_VM_FOLDER_H
