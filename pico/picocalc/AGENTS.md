# pico/picocalc - the PicoCalc runtime

The runtime for the ClockworkPi PicoCalc: an RP2040 Pico H in a handheld with
a 320x320 LCD, a keyboard, an SD card slot, speakers and a battery. It raises
the clock to 250MHz and starts a machine whose configuration is on the SD
card, chosen at power on, or without one boots MS-DOS 3.30 from an image on
the card or the one in flash, with a hard disk image if the card has one.
The guest's screen and the status rows below it are drawn by the second core
while the first runs the machine, paced to real time. It takes the
PicoCalc's keyboard as its own and plays the PC speaker. DOS's date and time
questions are answered by watching the screen, as `pico/bench` does - there
is no clock to set them from, and it keeps the boot the `dos-boot` workload.

Like `pico/bench` it uses only the core's public interface and is not built
in CI. Measuring follows `pico/bench/AGENTS.md` and the root `AGENTS.md`.

## The hardware

From ClockworkPi's repository, <https://github.com/clockworkpi/PicoCalc>:
the mainboard schematic, the samples under `Code/`, and the ST7365P
datasheet. Pinouts online disagree, and one has the audio pins wrong, so
check a pin against that repository.

| part | connection |
| --- | --- |
| LCD: ST7365P, 320x320 | SPI1's pins, driven by PIO. SCK 10, MOSI 11, MISO 12, CS 13, DC 14, RST 15 |
| Keyboard and power: an STM32 | I2C1. SDA 6, SCL 7, address `0x1F` |
| SD card | SPI0. MISO 16, CS 17, SCK 18, MOSI 19; card detect 22 |
| PSRAM, 8MB | PIO SPI. CS 20, SCK 21, MOSI 2, MISO 3 |
| Speaker | PWM on GP26 and GP27, filtered and amplified on the mainboard |

## Building and flashing

```sh
PICO_SDK_PATH=/usr/share/pico-sdk ./pico/picocalc/build.sh
./pico/bench/run.js build-pico/picocalc/yax86_picocalc.uf2
```

- **Output is logfmt over USB serial** (`core/AGENTS.md`): a `boot_stage`
  line per stage, then `event=done`, said again to a host that attaches
  later. USB CDC drops what is written with no host, so nothing is. The
  status is the answer to the `report` command, which `run.js` sends at
  `done`, so it reads this image as it does the bench.
- `src/runtime_log.c` holds the loggers: the machine's at `error`, as the
  bench's, the runtime's own modules at `info`, and a third for core 1, since
  a logger builds lines in its own buffer. The SDK's stdio locks around each
  write, so lines come out whole.
- **Flashing replaces ClockworkPi's factory firmware** - a boot loader at
  offset 0, PicoMite at 200KB, and PicoMite's storage at 968KB. Back it up
  before the first flash:
  ```sh
  picotool save -a picocalc-factory-flash.bin
  picotool load -f -x -o 0x10000000 picocalc-factory-flash.bin
  ```
  The factory firmware has no picotool reset interface; opening its serial
  port at 1200 baud reboots it into BOOTSEL: `stty -F /dev/ttyACM0 1200`.
- `YAX86_PICOCALC_DRAW_SCREEN=0` builds an otherwise identical image that
  does not draw the guest's screen, into `build-pico/picocalc-no-draw`, which
  is how drawing's cost is measured.
- `YAX86_PICOCALC_SYS_CLK_KHZ` sets the clock, 250000 by default; empty is the
  stock 125MHz, and past 400000 is refused.
- **The Pico is powered by its USB cable**: a crashed board needs the cable
  replugged, not the PicoCalc's power button. The PicoCalc's own power must be
  on for the keyboard controller and SD card.

## The dos-boot invariant

- **The machine is the bench's with a second floppy drive**, and the drive
  is what the boot's count differs by: the BIOS and DOS look at drive B,
  772 instructions off the floppy alone (2,328,900) and 757 with the hard
  disk. One drive, the bench's machine, reaches `A>` in 2,328,128
  instructions, as `pico/bench` reports, which is what says the two run the
  same machine. At 250MHz, off
  the floppy in flash, it takes 4.72 seconds of processor time without
  drawing and 4.82 with it - 123% and 121% of a real PC/XT - and 4.89 off the
  card; at 125MHz, 9.44 with drawing, 61.5%.
- If the count moves and the bench's does not, the runtime has stopped
  running the measured machine. A key pressed during the boot moves it, which
  is why keys queued before the firmware started are dropped. So does a
  floppy that is not `resources`' byte for byte, or a hard disk image - see
  the SD card section. Compare against a baseline with the same drives.

## Running in real time

- **The machine is paced between batches, and nothing else**: after each
  batch of 48,000 emulated cycles, `PaceToRealTime()` sleeps until real time
  catches up. The batches are the same either way, so pacing cannot change
  what the guest sees.
- Due times come from an anchor where the two clocks agreed, moved forward a
  whole emulated second at a time, so nothing overflows or rounds away.
- **A machine that falls behind catches up by at most 100ms**, like the SDL
  runtime: a short stall, such as a scroll, is made up flat out; a longer one
  is dropped and the anchor moved, lost from the guest's clock and counted as
  `dropped_us`. Drops come in steps of just over 100ms, so builds a few
  milliseconds apart can drop a step apart - compare processor time, not
  drops.
- **Speed is processor time** - elapsed less waited, `busy_us` in the boot
  line and each `boot_stage` - since paced, the boot takes as long as a real
  PC/XT. It matches an unpaced build to within 0.1%.
- **Faster on average is not faster throughout**: at 250MHz the boot drops
  0.40 seconds, 0.30 without drawing. Time prompt to `A>` is DOS printing and
  scrolling at 85% of real time, 91% without drawing - emulation, not drawing,
  holds it back. At 125MHz it drops 3.67 seconds.
- **The idle skip is turned on at the prompt**, not from power on, which would
  move the invariant. Without it the prompt runs at about 83% of real time;
  with it the processor is about 19% busy and emulated time tracks real time.
- The status page's Clock line compares the guest's time of day (the BIOS tick count
  at `0040:006C`) with real time since power on: about a quarter second
  behind from POST, and further only by what was dropped.
- **The overclock is made with the watchdog armed, after USB is up**, as in
  `pico/bench`, core voltage at 1.20V first: a clock the chip cannot take
  reboots it, and the next boot runs stock and reports `overclock skipped -
  the last attempt hung`.
- **The watchdog is disarmed once the change is survived, and must not be fed
  and left running**: `picotool reboot` only shortens its countdown, which
  feeding reloads, so the reboot never comes while `picotool` reports
  success. A later hang is recovered with the power button.

## The LCD

- The register values are ClockworkPi's, but RGB565 rather than their RGB666 -
  two bytes a pixel rather than three.
- **The serial interface is driven by PIO, not the SPI block.** `src/lcd.pio`
  is two instructions, a bit every two cycles, the clock on a side-set pin.
  The SPI block runs from the peripheral clock, which `set_sys_clock_khz()`
  moves to the 48MHz USB PLL on an overclock - see `set_sys_clock_pll()` in
  the SDK's `clocks.c` - capping the LCD at 24MHz; keeping it on the system
  clock (`PICO_CLOCK_ADJUST_PERI_CLOCK_WITH_SYS_CLOCK`) takes the keyboard's
  I2C and the card's SPI to 250MHz too. PIO runs from the system clock, which
  the overclock raises anyway.
- **The serial clock is 62.5MHz** at 125MHz and 250MHz. The datasheet rates
  writes at about 15MHz and ClockworkPi asks for 25MHz; it draws cleanly at
  62.5MHz, and at 125MHz that took drawing from 7.4% of `dos-boot` to 5.2%.
- **The control lines are held 50ns around every transfer**, covering the
  datasheet's write timings, the tightest CS high for 40ns. At 250MHz without
  it, grey text came out green through PIO - bytes swapped - and the SPI
  block drew nothing, which looked like the SPI block or the rate failing and
  was neither: with the hold, the SPI block draws at every rate tried from
  31.25 to 62.5MHz.
- **`LcdWaitIdle()` waits for the FIFO to empty before clearing the stall
  flag**: the state machine was stalled before the last byte was queued, so a
  flag cleared earlier can be set by the old stall and end the wait early.
- A write is `LcdBeginWrite()`, any number of `LcdWritePixels()`, then
  `LcdEndWrite()`: window commands, DC and CS once per window, since the core
  hands pixels over 32 at a time. Raising CS does not end a memory write -
  the next command does.

## The guest's screen, and what drawing it costs

- **Text renders 320x250 in the 4x10 font**, handed to the core through
  `YPlatformConfig.video_text_font`, in the top 250 rows; the status rows
  take the bottom 30. The core renders only what changed, as rectangles, and the panel
  keeps its own frame memory, so each region goes straight to it:
  `begin_render_region` opens a window and `write_pixels` streams RGB565.
- **The CGA's graphics modes render 640x200 and are drawn at 320x250**, in
  the same area: two pixels across to one, four rows down to five, every
  fourth row twice. That fills the area, and 320x250's 1.28 is close to a
  4:3 monitor's 1.33. The core's graphics dirty rows are four scan lines, so
  a region always maps to whole panel rows. The 320x200 modes double every
  pixel, so halving loses nothing; in 640x200 a pair is drawn as the color
  between them, keeping a one-pixel line at half brightness. A graphics row is
  assembled in a buffer, since a doubled row is sent twice; text goes
  straight through.
- **Core 1 draws everything on the LCD, and core 0 never touches it**
  (`src/display.c`). Core 0 hands over a copy of the guest's screen from
  `YVideoCopyForRender()`, the status values as text, and requests to turn
  the panel off and on or read it back, one word through the inter-core FIFO
  each.
- **Each buffer handed over has a flag only core 0 sets and only core 1
  clears**: set once filled, cleared once drawn, and left alone by core 0
  while set, so neither core waits. Drawing figures both write are under a
  lock.
- A frame is offered once per emulated frame, 60 a second, between batches.
  One offered while core 1 is still drawing is not copied and counts as late;
  what changed goes with the next. Four are late during `dos-boot`.
- **Drawing costs core 0 1.5% of `dos-boot`** at 250MHz: 4.7975 seconds
  against 4.7267 without, the same instructions, a spread under 0.01%. On
  core 0 it cost 5.3%. Of the 0.071 seconds, 0.010 is copying 16KB of video
  memory 70 times; 0.037 is core 1 drawing at the same time, sharing the XIP
  cache and SRAM with code that runs from flash (a build that skips drawing
  with the code linked measured 4.7607); and 0.024 is flash layout.
- Core 1 spends 0.271 seconds in `YVideoRender()` over 70 frames and 366,320
  pixels, against 0.245 when core 0 drew alone. **That moves with flash
  layout**: blanking code that runs only after the boot once took drawing at
  125MHz from 5.2% to 4.8%. Compare against a baseline built from the same
  code.
- **What is left is processor time, not the bus**: the pixels take 0.094
  seconds on the wire at 62.5MHz, and the rest - about 115 cycles a pixel -
  is the text renderer, a callback per 32 pixels and the RGB565 conversion,
  all running from flash.
- A scrolling screen redraws all 80,000 pixels a line, about 53ms at 250MHz;
  the machine runs on, and the frames due are drawn together. Putting drawing
  code in SRAM has not been tried. Nor has timing a full graphics frame.
- **The screen is blanked 30 seconds after the boot or the last key**, with
  the panel's display off and sleep in, `LcdSetDisplayOn(false)` - a static
  image can mark the panel. Nothing is rendered while blank; changes wait in
  the live state. A key turns it back on - the panel keeps its frame memory,
  so only changes are drawn - and goes on to the guest. The backlight is the
  controller's and will not dim below 16 of 255, so blanking is the panel's
  job.

## The keyboard controller

- **It answers a register at a time**: write the register number, read two
  bytes. ClockworkPi's 16ms wait between is not needed - the answer is ready
  as the number arrives - except for the version read at startup. **Write a
  register number before every read**: firmware before April 2025
  (`f58e85e`) freezes SCL otherwise, which is exactly what a bus scan does.
- **The bus runs at 10kHz**, as in ClockworkPi's samples. Every read
  succeeded at 10, 100 and 400kHz alike; a 10kHz read, about 5ms, finishes
  well within the batch that collects it.
- **The key FIFO is read without waiting, once a batch, on core 0**:
  `SouthbridgePollKey()` queues a whole read in the I2C block's FIFO, and the
  next call collects it and queues another - about 3.5us a batch, 2.1ms over
  `dos-boot`. An unacknowledged read aborts and is counted; one that never
  completes resets the block after 50ms. A read a batch is about a hundred a
  second against a 31-key FIFO, so typing never comes near it.
- **The controller queues keys whatever reads them, and runs on while the
  RP2040 restarts**, so its FIFO can be full of old keys - the first boot with
  the poll typed 31 of them and moved the invariant. Everything until the
  first read that finds it empty is dropped.
- **The poll's cost to `dos-boot` is layout**: 0.42% slower than master, of
  which a build whose poll returns at once accounted for 18ms of 20.
- **The version register answers from v1.6** (`410f8fc`, March 2026); earlier
  firmware answers unknown registers with zeros, reported as "older than
  v1.6". **v1.7 reports itself as v1.6**: its one change did not touch
  `BIOSVERSION`. The battery register answers
  `[0B, percent]`.
- **It can stop acknowledging for minutes**: the development unit, then
  older than v1.6, acknowledged nothing for two boots, then everything, cause
  unknown. v1.6 restarts its I2C after 2.5 quiet seconds, so the boot-time
  read retries for three; the key poll just tries again each batch.
- **Flashing it** is ClockworkPi's procedure from their wiki: DIP switch 1
  on the back, power on, and `stm32flash` through the USB-C port's CH340
  (`1a86:7523`). Read the old firmware back first - `stm32flash -r old.bin -S
  0x08000000:131072 /dev/ttyUSB0` - and the new one after, since `-v` does not
  check the whole image. The development unit came with v1.4 (byte for byte
  `Bin/PicoCalc_BIOS_v1.4.bin`) and runs v1.7.
- **After flashing it may not power on until it loses power completely** -
  both cables and the batteries out. A long press shuts the device down, so
  it is no way to try.

## The keyboard

- The controller reports characters, not keys: ASCII, with codes above 0x80
  for keys ASCII lacks, each pressed, held or released. `src/keymap.c` turns
  them into set 1 scancodes.
- **Shifted characters arrive shifted** - `!`, not Shift and 1 - and Shift,
  Ctrl and Alt also arrive as keys. So a character is sent as the PC key that
  types it, both cases alike, and modifiers as they come: the guest sees
  Shift held and the 1 key and makes the `!` itself.
- **PC keys the PicoCalc lacks arrive as shifted forms of others**: F6-F10 on
  F1-F5, Home on Tab, End on Delete, Page Up and Down on Up and Down, Insert
  on Enter (and Alt+I), Break on Esc. On a PC/XT these are keypad or
  separate keys that Shift would change - Shift and keypad Page Up is a 9 - so
  each goes as a whole press and release, with any Shift and Alt the guest
  thinks are held let go around it, and its release sends nothing. Ctrl is
  left alone, so Ctrl+Shift+Esc is Ctrl-Break.
- A held key repeats as make codes, from 300ms, ten a second.
- **Releases need not match presses**: the controller works the character
  out again on release, with the modifiers held then, so Shift let go before
  the 1 releases `1` - the same PC key, so correct. For the shifted forms it
  is not, the other reason their releases send nothing. The leftover case -
  arrow, Shift, arrow released - reports Page Up released, so the arrow is
  never broken; a PC/XT BIOS ignores a keypad key's release.
- Alt with comma, full stop, space and B belong to the controller (backlights,
  battery) and are never reported; Shift with Left or Right reports nothing.

## Machines

- **A machine is a configuration file naming its drives' images**,
  `src/machine_config.c`. Every `.CFG` in `yax86/machines` is one, named
  for the file. Each line is logfmt, `#` starts a comment, and a path is
  relative to the file's folder - `..` followed - or to the card's root
  with a leading `/`, so machines can share images and keep their own:

  ```
  # yax86/machines/DOS33.CFG
  drive_a=../MSDOS330/DISK01.IMG
  drive_b=../MSDOS330/DISK02.IMG
  drive_c=DOS33.IMG
  ```

  `drive_a` and `drive_b` are 360KB floppies, `drive_c` and `drive_d` the
  hard disk controller's master and slave, each the core's 10MB drive. A
  drive not named is empty.
- **`yax86/yax86.cfg` holds the card's settings**: `boot_default`, the path
  of the machine to highlight and start, relative to its own folder or the
  root, and listed even when it is outside `yax86/machines`; and
  `boot_delay`, the seconds the list waits for a key, 5 without it, 0 to
  start the default without showing the list.
- **At power on, with machines on the card, the menu lists them**, before
  the machine exists: the countdown runs on the list's bottom edge until a
  key is pressed, then the default starts. A file that does not read keeps
  the list up with its problem on the keys row - `BROKEN.CFG: line 2: no
  setting drive_e` - as do bad settings, which start the countdown anyway
  with what was read. The countdown is what lets a board left alone, or a
  host measuring the boot, still boot.
- **A machine's image that does not open leaves its drive empty**, and the
  message row says why after the start; so does one named for two drives,
  which goes in the first. There is no falling back to the built-in floppy
  once a machine names its drives.
- **Without machines, the drives are as before them**: drive A is
  `yax86/floppy_a.img`, or the read-only floppy in flash, with the status
  page's Disk line saying why - no card, no answer, no FAT volume, no image,
  or the wrong size - and drive C is `yax86/hdd.img` if present.
- Nothing yet writes a configuration: a disk changed in the menu is
  forgotten at the next power on.

## The SD card, the floppy drives and the hard disk

- **Images are on the card's FAT volume**, and guest writes go to the card.
  The card is mounted once, at power on.
- **Drive B starts empty.** With two drives DOS has a B: of its own
  rather than one it shares with A, prompting for a disk swap.
  `YPlatformConfig.floppy_drives` sets what the BIOS's DIP switches report.
- **The menu changes the disk in either drive** for any 360KB image on the
  card, or ejects it. `DiskImageClose()` writes back the sector held and
  closes the file; the FDC then reports the drive not ready, which DOS
  shows as "General Failure ... Abort, Retry, Fail?". The built-in floppy is
  not offered again once ejected - a reset brings it back.
- **An image goes in one drive at a time.** Each drive keeps its own copy of
  a sector, so two drives writing one file would each overwrite the other's
  changes; inserting an image the other drive holds is refused with a
  message.
- `src/file_list.c` lists a folder for the menu's browser: `..`, folders,
  then files exactly the image size, each by name, with hidden and system
  entries left out. It holds 64, and says when a folder had more. Names are
  8.3, as FatFs is built without long names, and paths are FatFs's, with no
  leading slash.
- **A hard disk image is the core's 10MB drive**, 306 cylinders, 4 heads, 17 sectors, exactly
  10,653,696 bytes. A blank one is `dd if=/dev/zero of=hdd.img bs=512
  count=20808`, partitioned with FDISK and formatted with `FORMAT C: /S` from
  the boot floppy. Nothing on the device creates one. Its partition starts
  at sector 17, so mtools reach it as `hdd.img@@8704`.
- `src/sdcard.c` runs the card in SPI mode on SPI0, from the SD simplified
  physical layer spec: 400kHz to bring it up, then 24MHz - the 48MHz
  peripheral clock divided evenly, under the 25MHz default speed limit. Each
  command starts from a fresh select that waits for ready; blocks move one
  at a time, CMD17 and CMD24.
- **The filesystem is FatFs R0.16.** `resources/FatFs/ff16.zip` is the
  release exactly as elm-chan.org serves it, unpacked by `FetchContent` and
  checked against its SHA-256, which says it is unchanged and keeps the tree
  to our own code. To upgrade, replace the zip and hash and carry our
  settings over.
- **`src/ffconf.h` is the release's with four changes**: code page 437,
  `FF_FS_TINY`, `FF_FS_NORTC` and `FF_USE_FASTSEEK`; long names and exFAT are
  off. `configure_file()` copies it over the stock one - `ff.h` includes it
  from its own directory first - and reconfigures when it changes.
- FatFs is the only thing in the image that calls `memcpy()` and `memset()`.
- **Both controllers move a byte at a time, and `src/disk_image.c` keeps one
  sector of each image**, so a sector costs one card access. A written sector
  goes to the card as soon as its last byte is in, so nothing waits to be
  lost at power off. `src/floppy.c` and `src/harddisk.c` each hold an image.
- **Each image keeps a cluster map, FatFs's fast seek**: without it a seek
  backwards walks the chain from the start, and a hard disk seeks backwards
  constantly. It has room for 31 fragments; a more fragmented image goes
  without, shown as `fast_seek` in the report.
- **The card costs 1.5% of `dos-boot`**: 4.8896 seconds against 4.8171 from
  flash. DOS reads 214 sectors, 75ms, about 0.35ms a sector.
- **The invariant holds only off an image identical to `resources`', with no
  hard disk**: a copy of `DISK01.IMG` boots in 2,328,128, but with a file
  added 2,328,239. The hard disk's BIOS and DOS's partition read change the
  boot more: 2,398,153 instructions with a blank drive, 2,415,480 formatted
  with one floppy drive; the second floppy drive adds 757.
- **Code added here can cost the boot without running**: the hard disk made
  the boot 0.85% slower with no image on the card, though the floppy's reads
  got faster. A third of it is guest RAM moving - the images are 8-byte
  aligned, and `.bss` sorts by alignment before name, so they land ahead of
  it - and the rest most likely flash layout.

## The speaker

- **The PC speaker plays on both speakers** from `src/speaker.c`. The core
  reports a square wave's frequency, or 0, and one PWM slice makes exactly
  that: GP26 and GP27 are its two channels, the period set to the tone and
  both duties to half. The registers are written only when the tone changes.
- The divider is the smallest that lets the 16-bit counter reach the tone,
  so the PIT's lowest, 18Hz, plays - a divider of 212 at 250MHz. Above 20kHz
  is played as silence, unheard anyway.
- **A tone starts and stops at the batch it was set in**, moving each edge by
  up to a batch, about 10ms: inaudible in a tune, but a click a few
  milliseconds long comes out the wrong length.
- The report's `speaker` group has the tone asked for, the tone played -
  how a test checks it without listening - and the number of changes; the
  POST beep is two.
- Clicks and digitized sound made by toggling the speaker bit cannot play:
  the core reports a frequency, not the speaker line - see
  `YPlatformConfig.set_pc_speaker_frequency`.

## The status rows

- **Three rows of the 4x10 font at the bottom of the panel**, composed by
  `src/status_bar.c` on core 0 and drawn by core 1, which draws only the
  rows that changed. A line a pixel high in dark grey, two pixels above
  them, sets them off from the guest's screen; the rows between are black.

  ```
  Inserted GAMES.IMG in drive B                                         <- messages
  A:DISK01.IMG  B:GAMES.IMG  C:DOS33.IMG                                <- drives
  DOS33             CAPS NUM SCRL INS     00:44:51        ↑+↑=Menu  [████] 100%
  ```

- **Messages** are queued and shown four seconds each, without moving
  anything on the rows below. While the menu is open the row shows its
  keys, and while the machine is dropping time it shows `SLOW` and its
  speed in yellow.
- **Keys are labeled as MS-DOS's own programs label them**, `Key=Action`
  pairs two spaces apart - `↑↓=Move  Enter=Choose  Esc=Back` - the action
  brighter than the key, so the two read apart in the 4x10 font. The menu
  key on the bottom row, `↑+↑=Menu`, is the same a step dimmer.
- **Drives** follow each other two columns apart: the image's file name,
  without its folders, in capitals, `built-in` for the floppy in flash, or
  `empty` for a floppy drive without a disk. A hard disk with no image is
  left out. The card keeps 8.3 names, so a name fits; a longer one would
  keep 11 characters and end in `~`. The letter is the drive's light, green
  for 0.2 seconds after a sector is read or written.
- **Indicators**: the machine's name, if one was started; the guest BIOS's
  own lock and insert flags, at
  `0040:0017` - a PC/XT keyboard has no lights - bright when on; the
  guest's time of day; the menu's key; and the battery, from the keyboard
  controller's register `0x0B`, which the key FIFO poll reads every ten
  seconds in its place. The controller measures it every 20 seconds, and
  answers 0% with no battery, which is shown as no battery. While the list
  of machines shows at power on there is no guest and the menu key does
  nothing, so the row has only the battery.
- **They are composed ten times a second**, so that a light comes on while
  its drive is in use, and composing is skipped while nothing they show has
  changed. Composing every time cost the boot 0.74% against master; skipping
  it, 0.42%, within 0.06% of composing once a second.
- The status page in the menu has the six lines of figures the panel used
  to show at all times.

## The menu

- **Both Shift keys held together open it**, which nothing in DOS does and
  typing never does, and close it again. `src/menu.c` keeps its state and
  draws it; `main.c` carries out what its items do, through the callbacks
  in `MenuConfig`.
- **The machine is paused while it is open**: no batches run and no frames
  are drawn, and on closing, pacing starts again from the present rather
  than running flat out to catch up. The time is `paused_us` in the report,
  and counts as neither busy nor waited. The guest's clock stands still.
- The guest has seen both Shift keys go down when the menu opens, and would
  not see them come up while the menu has the keyboard, so they are released
  to it then.
- **It is a box drawn over the guest's screen**, white on blue in code page
  437's double lined box, a page at a time: Drive A, Drive B, Status and
  Reset - Esc resumes; a drive's page, with what is in it, Swap disk image
  and Eject, or Choose disk image alone while it is empty; the browser; the
  status page; and a question before a reset. An item that opens another
  menu ends in a `>`. Up and Down move, Enter or Space chooses, Esc or
  Backspace goes back.
- **The browser starts in the folder of the image in the drive**, with it
  chosen, or else of the image in the other drive - the next disk of a set
  is likely beside it - or else in `yax86`. Each image in a drive is marked
  with its drive. Enter opens a folder or inserts an image and closes the
  menu; Backspace goes up a folder, with the one left chosen, and from the
  root back to the drive's page; Page Up and Down move a page.
- **A box of a new size has the guest's screen drawn in full under it
  first**, and closing draws it in full, so nothing of a box is left behind.
- **Reset restarts the board** with `watchdog_reboot()`, which is the
  machine from power on. What the guest wrote is already on the card.
- **A test drives it as a hand would**, with `key shift press`, `key rshift
  press` and their releases, and checks it with `screenshot`; the report's
  `menu` group says whether it is open and which page shows.

## Commands over USB

- **A host drives the runtime with commands on the same serial port**, a
  logfmt line each - `id=7 cmd=screen` - answered by `kind=reply` lines with
  that id, the last with `status=ok`, or `status=error` and an `error`.
  `src/command.c` reads them and `main.c` carries them out:

  | command | does |
  | --- | --- |
  | `ping` | answers, and nothing else |
  | `report` | a line for each group of the status - `system`, `boot`, `display`, `drive_a` to `drive_d`, `machine`, `keyboard`, `speaker`, `menu`, `pacing` |
  | `screen` | the mode and the cursor, then each row's characters as hex - and with `attrs=true`, their attributes |
  | `screenshot` | the panel read back, 320x320 in RGB565, as runs of one color in hex |
  | `key name=... [state=press\|release]` | presses, releases or, by default, taps a key by name |

- **Commands are read once a batch without waiting**, up to 64 bytes, and run
  between batches. With nothing sent, the poll costs about 6us a batch. Time
  spent on commands is `command_us` in the report.
- **A screen row is hex**: a row of box drawing characters escaped as logfmt
  is 320 characters, past the logger's 256-byte line.
- **`key` is the only way in, so a host can do exactly what a hand on the
  keyboard can.** A key is the controller's: a character names the key that
  types it, and `shift`, `ctrl` and `alt` are keys, so `A` is Shift around a
  tap of `a`. Typing text is the host's job. Nothing can send a key the
  PicoCalc lacks, so no test does what a user could not.
- **A key goes through the keymap, one event a batch**, from the queue in
  `src/typing.c`, in a batch the real keyboard had none. The reply comes
  once it is queued, not once the guest has it: a test waits for the key's
  effect on the screen. A full queue refuses with `queue_full`, queuing
  nothing, and `debug.js` resends - though a host typing a key at a time
  never gets there: a queue cut to two events refused nothing.
- **A screenshot is the panel's frame memory read back**, so it shows what
  the panel holds, drawn right or wrong. Core 1 owns the panel, so it reads
  and replies, drawing nothing for the half second it takes - frames due
  meanwhile count late. The read goes through the SPI block at 6MHz (the
  datasheet's read cycle is 150ns), the pins handed over from PIO and back.
  The panel answers in the format it is written in, RGB565, after a dummy
  byte.
- **A screenshot is sent as runs of one color**: `runs=` lines of six hex
  digits a run, the RGB565 color then the length less one. A text screen is
  29KB of runs against 205KB of pixels. A pixel at a time it took 1.41
  seconds - 0.33 reading, 0.34 formatting, 0.73 sending 546KB - and as runs
  about half a second, mostly the read, which the panel bounds. An index of
  recent colors, as QOI keeps, would halve the data again but save little
  time; deflate needs RAM the board lacks.
- **`debug.js` is the host's side**: `screen`, `screenshot FILE.png`,
  `type`, `key`, `wait`, `report`, `ping`, or `run` with a script of them, a
  step a line. `type` sends the keys that type its text, Shift included.
  `wait` polls the screen until a regular expression matches, a graphics mode
  counting as no match yet; a timeout fails the script, which makes it a
  test:

  ```
  type CLS\r
  wait ^A> *\n(?![\s\S]*\S)
  type DIR C:\r
  wait bytes free
  ```

  A wait sees the whole screen and can run before the guest has the keys, so
  wait for something only the new input produces - after CLS, the prompt
  alone on a blank screen, not any prompt. The port is opened once a session;
  one already there is used at once, and one that just appeared after a
  flash gets 800ms to settle.
- **A command goes out with a newline before it as well as after.** A tty
  opens with echo on until it is made raw, so what the board says then - to
  `serial.js` before it is raw, or to `stty` or anything else that opens the
  port - is echoed back without its line end. The next command was read as
  the rest of that line and lost, two runs in five. The leading newline ends
  the echo's line instead; the board answers it with an error under id 0,
  which nothing waits for, and ignores empty lines.

## The 4x10 font

- 80 columns in 320 pixels is four pixels a character, and a 640-pixel text
  screen shrunk to fit is unreadable. What works is a font drawn for the
  cell - C64 80-column software used 4x8.
- `src/font_4x10.c` is the only complete code page 437 font of that width
  taller than five rows that turned up. Its ASCII glyphs are rugosi's, from
  picocalc_runCPM, under MIT - the notice is in the file - and the rest,
  every box drawing and block character included, Serg Podtynnyi's under
  GPLv3-or-later.
- Every glyph outside the box drawing range sits one row lower than the box
  drawing ones, leaving a blank row between lines of text while box drawing
  fills the cell and joins. The shift is in the table, so it is a plain font.
  The three glyphs using the bottom row - `0x02`, `0x0F`, `0x16`, cell-filling
  patterns - are not shifted.
