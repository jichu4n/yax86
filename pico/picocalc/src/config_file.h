// Configuration files on the SD card: logfmt lines of key=value pairs, any
// number to a line, with a line starting with # a comment.
//
//   # Settings for the whole card
//   boot_default=vm/dos330/vm.cfg
//   boot_delay=5
//
// A file is read whole, its pairs handed one at a time to whatever
// understands them, and updated by setting keys to new values with everything
// else in it - comments, other pairs, line ends - left as it was.
#ifndef YAX86_PICOCALC_CONFIG_FILE_H
#define YAX86_PICOCALC_CONFIG_FILE_H

#include <stdbool.h>
#include <stdint.h>

enum {
  // The longest file read or written.
  kConfigFileMaxBytes = 2048,
  // The longest problem described.
  kConfigFileProblemLength = 60,
  // The most settings one update sets.
  kConfigFileMaxSettings = 32,
};

// What a file's pairs are handed to, one at a time. Returns false, with the
// problem described, at one that is not understood.
typedef bool (*ConfigFileTakePair)(
    const char* key, const char* value, void* context,
    char problem[kConfigFileProblemLength + 1]);

typedef enum ConfigFileResult {
  kConfigFileOk = 0,
  // No card, or no file at the path: for the caller to take as defaults.
  kConfigFileMissing,
  // The file could not be read, or has a line that is not understood.
  kConfigFileProblem,
} ConfigFileResult;

// Read the file at path, from the card's root, handing each pair to take in
// the order they are written. Anything but kConfigFileOk comes with the
// problem described - one in a line naming its line number, "line 2: ...".
ConfigFileResult ConfigFileRead(
    const char* path, ConfigFileTakePair take, void* context,
    char problem[kConfigFileProblemLength + 1]);

// A key to set, and its new value, written quoted if logfmt needs it to be -
// or NULL to remove the key's pairs, and with them a line left with nothing
// else on it.
typedef struct ConfigFileSetting {
  const char* key;
  const char* value;
} ConfigFileSetting;

// Set each key to its value in the file at path, creating the file if there is
// none, for up to kConfigFileMaxSettings settings. Every pair with the key
// takes the new value where it stands - the last read is the one that counts -
// and a key with no pair gains a line of its own at the end. Everything else in
// the file stays as it was.
//
// The new file is written in full beside the old, with the extension .new,
// before taking its place, so a write cut short leaves the old one. Returns
// false, with the problem described, if the file could not be read, would be
// longer than kConfigFileMaxBytes, or could not be written.
bool ConfigFileUpdate(
    const char* path, const ConfigFileSetting* settings, uint8_t count,
    char problem[kConfigFileProblemLength + 1]);

#endif  // YAX86_PICOCALC_CONFIG_FILE_H
