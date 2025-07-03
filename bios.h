// ==============================================================================
// YAX86 BIOS MODULE - GENERATED SINGLE HEADER BUNDLE
// ==============================================================================

#ifndef YAX86_BIOS_BUNDLE_H
#define YAX86_BIOS_BUNDLE_H

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// ==============================================================================
// src/bios/public.h start
// ==============================================================================

#line 1 "./src/bios/public.h"
// Public interface for the BIOS module.
#ifndef YAX86_BIOS_PUBLIC_H
#define YAX86_BIOS_PUBLIC_H

#include <stdint.h>

// ============================================================================
// BIOS state
// ============================================================================

enum {
  // Address of the BIOS Data Area.
  kBDAAddress = 0x0040,

  // Text mode framebuffer address.
  kTextModeFramebufferAddress = 0xB8000,
  // Number of columns in text mode.
  kTextModeColumns = 80,
  // Number of rows in text mode.
  kTextModeRows = 25,
  // Size of the text mode framebuffer in bytes. 2 bytes per character (char +
  // attribute).
  kTextModeFramebufferSize = kTextModeColumns * kTextModeRows * 2,
};

// BIOS Data Area (BDA) structure.
typedef struct BDA {
  // 0x00: Base I/O address for serial ports.
  uint16_t com_address[4];
  // 0x08: Base I/O address for parallel ports.
  uint16_t lpt_address[4];
  // 0x10: Equipment word.
  uint16_t equipment;
  // 0x12: POST status / Manufacturing test initialization flags
  uint8_t post_status;
  // 0x13: Base memory size in kilobytes (0-640)
  uint16_t memory_size;
  // 0x15: Manufacturing test scratch pad
  uint8_t manufacturing_test_1;
  // 0x16: Manufacturing test scratch pad / BIOS control flags
  uint8_t manufacturing_test_2;
  // 0x17: Keyboard status flags 1
  uint8_t keyboard_status_1;
  // 0x18: Keyboard status flags 2
  uint8_t keyboard_status_2;
  // 0x19: Keyboard: Alt-nnn keypad workspace
  uint8_t keyboard_alt_numpad;
  // 0x1A: Keyboard: ptr to next character in keyboard buffer
  uint16_t keyboard_buffer_head;
  // 0x1C: Keyboard: ptr to first free slot in keyboard buffer
  uint16_t keyboard_buffer_tail;
  // 0x1E: Keyboard circular buffer (16 words)
  uint16_t keyboard_buffer[16];
  // 0x3E: Diskette recalibrate status
  uint8_t diskette_recalibrate_status;
  // 0x3F: Diskette motor status
  uint8_t diskette_motor_status;
  // 0x40: Diskette motor turn-off time-out count
  uint8_t diskette_motor_timeout;
  // 0x41: Diskette last operation status
  uint8_t diskette_last_status;
  // 0x42: Diskette/Fixed disk status/command bytes (7 bytes)
  uint8_t diskette_status_command[7];
  // 0x49: Video current mode
  uint8_t video_mode;
  // 0x4A: Video columns on screen
  uint16_t video_columns;
  // 0x4C: Video page (regen buffer) size in bytes
  uint16_t video_page_size;
  // 0x4E: Video current page start address in regen buffer
  uint16_t video_page_offset;
  // 0x50: Video cursor position (col, row) for eight pages
  uint16_t video_cursor_pos[8];
  // 0x60: Video cursor type, 6845 compatible
  uint16_t video_cursor_type;
  // 0x62: Video current page number
  uint8_t video_current_page;
  // 0x63: Video CRT controller base address
  uint16_t video_crt_base_address;
  // 0x65: Video current setting of mode select register
  uint8_t video_mode_select;
  // 0x66: Video current setting of CGA palette register
  uint8_t video_cga_palette;
  // 0x67: POST real mode re-entry point after certain resets
  uint32_t post_reentry_point;
  // 0x6B: POST last unexpected interrupt
  uint8_t post_last_interrupt;
  // 0x6C: Timer ticks since midnight
  uint32_t timer_ticks;
  // 0x70: Timer overflow, non-zero if has counted past midnight
  uint8_t timer_overflow;
  // 0x71: Ctrl-Break flag
  uint8_t ctrl_break_flag;
  // 0x72: POST reset flag
  uint16_t post_reset_flag;
  // 0x74: Fixed disk last operation status
  uint8_t fixed_disk_status;
  // 0x75: Fixed disk: number of fixed disk drives
  uint8_t fixed_disk_count;
  // 0x76: Fixed disk: control byte
  uint8_t fixed_disk_control;
  // 0x77: Fixed disk: I/O port offset
  uint8_t fixed_disk_port_offset;
  // 0x78: Parallel devices 1-3 time-out counters
  uint8_t parallel_timeout[4];
  // 0x7C: Serial devices 1-4 time-out counters
  uint8_t serial_timeout[4];
  // 0x80: Keyboard buffer start offset
  uint16_t keyboard_buffer_start;
  // 0x82: Keyboard buffer end+1 offset
  uint16_t keyboard_buffer_end;
  // 0x84: Video EGA/MCGA/VGA rows on screen minus one
  uint8_t video_rows;
  // 0x85: Video EGA/MCGA/VGA character height in scan-lines
  uint16_t video_char_height;
  // 0x87: Video EGA/VGA control
  uint8_t video_ega_control;
  // 0x88: Video EGA/VGA switches
  uint8_t video_ega_switches;
  // 0x89: Video MCGA/VGA mode-set option control
  uint8_t video_vga_control;
  // 0x8A: Video index into Display Combination Code table
  uint8_t video_dcc_index;
  // 0x8B: Diskette media control
  uint8_t diskette_media_control;
  // 0x8C: Fixed disk controller status
  uint8_t fixed_disk_controller_status;
  // 0x8D: Fixed disk controller Error Status
  uint8_t fixed_disk_error_status;
  // 0x8E: Fixed disk Interrupt Control
  uint8_t fixed_disk_interrupt_control;
  // 0x8F: Diskette controller information
  uint8_t diskette_controller_info;
  // 0x90: Diskette drive 0 media state
  uint8_t diskette_drive0_media_state;
  // 0x91: Diskette drive 1 media state
  uint8_t diskette_drive1_media_state;
  // 0x92: Diskette drive 0 media state at start of operation
  uint8_t diskette_drive0_start_state;
  // 0x93: Diskette drive 1 media state at start of operation
  uint8_t diskette_drive1_start_state;
  // 0x94: Diskette drive 0 current track number
  uint8_t diskette_drive0_track;
  // 0x95: Diskette drive 1 current track number
  uint8_t diskette_drive1_track;
  // 0x96: Keyboard status byte 3
  uint8_t keyboard_status_3;
  // 0x97: Keyboard status byte 4
  uint8_t keyboard_status_4;
  // 0x98: Timer2: ptr to user wait-complete flag
  uint32_t timer2_wait_flag_ptr;
  // 0x9C: Timer2: user wait count in microseconds
  uint32_t timer2_wait_count;
  // 0xA0: Timer2: Wait active flag
  uint8_t timer2_wait_active;
  // 0xA1: Reserved for network adapters (7 bytes)
  uint8_t network_reserved[7];
  // 0xA8: Video: EGA/MCGA/VGA ptr to Video Save Pointer Table
  uint32_t video_save_pointer_table;
  // 0xAC: Reserved (4 bytes)
  uint8_t reserved_ac[4];
  // 0xB0: ptr to 3363 Optical disk driver or BIOS entry point
  uint32_t optical_disk_ptr;
  // 0xB4: Reserved (2 bytes)
  uint8_t reserved_b4[2];
  // 0xB6: Reserved for POST (3 bytes)
  uint8_t reserved_post[3];
  // 0xB9: Unknown (7 bytes)
  uint8_t unknown_b9[7];
  // 0xC0: Reserved (14 bytes)
  uint8_t reserved_c0[14];
  // 0xCE: Count of days since last boot
  uint16_t days_since_boot;
  // 0xD0: Reserved (32 bytes)
  uint8_t reserved_d0[32];
  // 0xF0: Reserved for user (16 bytes)
  uint8_t user_reserved[16];
  // 0x100: Print Screen Status byte
  uint8_t print_screen_status;
} BDA;

// State of the BIOS.
typedef struct BIOSState {
  // BDA structure, located at kBDAAddress (0x0040).
  BDA bda;
  // Text mode framebuffer, located at kTextModeFramebufferAddress (0xB8000).
  uint8_t text_framebuffer[kTextModeFramebufferSize];
} BIOSState;

// Initialize BIOS state.
void InitBIOS(BIOSState* bios);

#endif  // YAX86_BIOS_PUBLIC_H


// ==============================================================================
// src/bios/public.h end
// ==============================================================================


#ifdef YAX86_IMPLEMENTATION

// ==============================================================================
// src/common.h start
// ==============================================================================

#line 1 "./src/common.h"
#ifndef YAX86_COMMON_H
#define YAX86_COMMON_H

// Macro that expands to `static` when bundled. Use for variables and functions
// that need to be visible to other files within the same module, but not
// publicly to users of the bundled library.
//
// This enables better IDE integration as it allows each source file to be
// compiled independently in unbundled form, but still keeps the symbols private
// when bundled.
#ifdef YAX86_IMPLEMENTATION
// When bundled, static linkage so that the symbol is only visible within the
// implementation file.
#define YAX86_PRIVATE static
#else
// When unbundled, use default linkage.
#define YAX86_PRIVATE
#endif  // YAX86_IMPLEMENTATION

#endif  // YAX86_COMMON_H


// ==============================================================================
// src/common.h end
// ==============================================================================

// ==============================================================================
// src/bios/bios.c start
// ==============================================================================

#line 1 "./src/bios/bios.c"
#ifndef YAX86_IMPLEMENTATION
#include "../common.h"
#include "public.h"
#endif  // YAX86_IMPLEMENTATION

void InitBIOS(BIOSState* bios) {
  // Zero out the BIOS state.
  BIOSState empty_bios = {0};
  *bios = empty_bios;
  // TODO: Set BDA values.
}


// ==============================================================================
// src/bios/bios.c end
// ==============================================================================


#endif  // YAX86_IMPLEMENTATION

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // YAX86_BIOS_BUNDLE_H

