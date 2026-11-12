# pico/picocalc - the PicoCalc runtime

The runtime for the ClockworkPi PicoCalc: an RP2040 Pico H in a handheld with
a 320x320 LCD, a keyboard, an SD card slot, speakers and a battery. It raises
the clock to 250MHz and starts a VM whose configuration is on the SD
card, chosen at power on, or with none boots MS-DOS 3.30 from the floppy
image in flash.
The guest's screen and the status rows below it are drawn by the second core
while the first runs the VM, paced to real time. It takes the
PicoCalc's keyboard as its own and plays the PC speaker. DOS's date and time
questions are answered by watching the screen, as `pico/bench` does - there
is no clock to set them from, and it keeps the boot the `dos-boot` workload.

Like `pico/bench` it uses only the core's public interface. CI builds it -
see `pico/bench/AGENTS.md`. Measuring follows `pico/bench/AGENTS.md` and the root `AGENTS.md`.

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
- `src/runtime_log.c` holds the loggers: the VM's at `error`, as the
  bench's, the runtime's own modules at `info`, and a third for core 1, since
  a logger builds lines in its own buffer. The SDK's stdio locks around each
  write, so lines come out whole.
- **Each core has a 4KB stack, core 1's directly below core 0's**, so core 0
  running past the bottom of its own writes over the frames core 1 is in,
  which hangs core 1 - drawing and screenshots - with nothing said. Both are
  painted at startup, and the report's `stack` group gives the most of each
  ever used. Core 0 peaks at about 3.2KB, renaming a VM other VMs name.
  Inlined into the list's loop, the handlers of its actions had all their
  locals on the stack at once, and a report at the list overflowed it; they
  are kept out of line with `__noinline`. The report is answered in three
  parts for the same reason: its fields take stack until the function they
  are in returns, 1.5KB for all of them. `-fstack-usage` gives each
  function's frame.
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

- **With no VMs on the card, the machine is the bench's with a second
  floppy drive**: the built-in floppy in drive A, B empty and no hard disk.
  It reaches `A>` in 2,328,818 instructions, the bench's 2,328,128 and 690
  for the BIOS and DOS looking at drive B, which is what says the two run
  the same machine. A VM's count depends on its images: DOS330, the two
  MS-DOS 3.30 floppies and a hard disk, takes 2,416,050. At 250MHz, off
  the floppy in flash, it takes 4.72 seconds of processor time without
  drawing and 4.82 with it - 123% and 121% of a real PC/XT - and 4.89 off the
  card; at 125MHz, 9.44 with drawing, 61.5%.
- If the count moves and the bench's does not, the runtime has stopped
  running the measured machine. A key pressed during the boot moves it, which
  is why keys queued before the firmware started are dropped. So does a
  floppy that is not `resources`' byte for byte, or a hard disk image - see
  the SD card section. Compare against a baseline with the same VM, or
  none.
- **These counts are the 128KB machine's**, which the runtime runs without
  the PSRAM or after a crash. With the PSRAM it pages 640KB (see "Paging
  guest RAM"), and DOS330 boots in 2,443,404 instructions and 29,160,390
  cycles, the BIOS testing and DOS sizing more memory; the bench's 640KB
  `dos-boot` takes 2,356,813 and 27,964,698. The runtime with no VMs has not
  been counted at 640KB.

## Running in real time

- **The VM is paced between batches, and nothing else**: after each
  batch of 48,000 emulated cycles, `PaceToRealTime()` sleeps until real time
  catches up. The batches are the same either way, so pacing cannot change
  what the guest sees.
- Due times come from an anchor where the two clocks agreed, moved forward a
  whole emulated second at a time, so nothing overflows or rounds away.
- **A VM that falls behind catches up by at most 100ms**, like the SDL
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
  take the bottom 20. The core renders only what changed, as rectangles, and the panel
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
  the VM runs on, and the frames due are drawn together. Putting drawing
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

## VMs

- **A VM is a folder in `yax86/vm`**, `src/vm_config.c`,
  holding `vm.cfg`, which names its drives' images, and the images that
  are its own. Every folder there with a `vm.cfg` is one, named for the
  folder, and there are no VMs anywhere else. Each line is logfmt, `#` starts a comment, and a path is
  relative to the file's folder - `..` followed - or to the card's root
  with a leading `/`, so VMs can share images and keep their own:

  ```
  # yax86/vm/dos330/vm.cfg
  fd1=../../images/msdos330/disk01.img
  fd2=../../images/msdos330/disk02.img
  hd1=hd1.img
  ```

  `fd1` and `fd2` are 360KB floppies, `hd1` and `hd2` the hard disk
  controller's master and slave, each the core's 10MB drive.
- **A drive can be left out of the machine**, by leaving it out of
  `vm.cfg`. Floppy disk 1 is always there, `fd1=""` or not named holding no
  disk. Floppy disk 2 not named is not there: the BIOS's switches report one
  floppy drive - POST shows `FDD [ 1 ]` - and DOS's B: is drive A by another
  name, asking for B:'s disk in it, as on a one-drive XT; `fd2=""` is a
  second drive with no disk. A hard disk with no image, not named or `""`,
  is not there either: the XTIDE BIOS finds no drive, and nothing can be put
  in one while the VM runs. A drive not there is "disabled" on the form and
  the VM's page, and left off the status rows and the in-VM menu.
- **The drives are floppy disks 1 and 2 and hard disks 1 and 2, FD1, FD2,
  HD1 and HD2, not A to D** - in `vm.cfg`, the menu, the status rows and
  the report. DOS gives letters to
  partitions: each disk's primary partition, first disk then second, then
  each one's logical drives. So a second partition on hard disk 1 comes after
  hard disk 2's first, and with hard disk 1 blank, hard disk 2's partition is
  C:. 1 and 2 are FDISK's own numbers for them. The floppies follow the same
  pattern, though DOS calls them A: and B:.
- **`yax86/yax86.cfg` holds the card's settings**: `boot_default`, the name
  of the VM to highlight and start - `boot_default=DOS330` - `boot_delay`,
  the seconds the list waits for a key, 5 without it, 0 to start the default
  without showing the list, and `font`, the 4x10 font (`ymini`, `atari800`,
  `miniwi` or `creep`, `ymini` without it).
- **At power on, with a card, the menu lists its VMs**, before the VM
  exists, then a separator line, Manage VMs - see below - USB disk mode -
  see "The USB device" - and Settings (which holds Font). The countdown runs on
  the list's bottom edge until a key is pressed, then the default starts.
  With no VMs the list has Manage VMs, USB disk mode and Settings, with no
  separator, and nothing counts down or starts by itself; the built-in floppy boots only without a
  card. Every page reached from the list is drawn the list's
  size, as there is no guest's screen yet to draw under a smaller box. A file that does not read keeps
  the list up with its problem on the keys row - `BROKEN: line 2: no
  setting drive_e` - as do bad settings, which start the countdown anyway
  with what was read. A `boot_default` naming no VM is one, and the
  first VM listed is the default instead, so a mistyped name still
  boots. The countdown is what lets a board left alone, or a
  host measuring the boot, still boot.
- **A VM's image that does not open leaves its drive empty**, and the
  message row says why after the start; so does one named for two drives,
  which goes in the first. There is no falling back to the built-in floppy
  once a VM names its drives.
- **Without VMs - no card, or none in `yax86/vm` - drive A holds the
  read-only floppy in flash** and no other drive has a disk, so a PicoCalc
  with no card still boots; without a card, at once, with no list. No image on the card is looked for by name:
  every image a VM uses is one its `vm.cfg` names.
- **`src/config_file.c` reads and updates every configuration file**:
  `ConfigFileRead()` hands each pair to the caller, and
  `ConfigFileUpdate()` sets keys to new values. An update changes only
  those pairs, where they stand: comments, other pairs and line ends stay,
  a key with no pair gains a line at the end, and a missing file is
  created. Values are written with the core's `YLogfmtLine`, so they are
  quoted exactly as `YLogfmtParse()` reads them back. The new file is
  written in full beside the old with the extension `.new` -
  `yax86/yax86.new` - before the old one is removed and it is renamed, so
  a write cut short leaves the old file.
- **The VM started becomes `boot_default`**, so the next power on
  highlights it and starts it after the countdown. `yax86.cfg` is written
  only when the default changes - a board that starts its default every
  time never writes. A failure says why on the message row and the VM
  starts anyway.
- **A floppy swapped or ejected in the menu is saved to the VM's
  `vm.cfg`**, so the next start has the same disks: `fd2=""` once
  ejected, and an insert that fails leaves the drive empty and saves it
  so. Without a VM there is no file, and nothing is saved.
- **Paths are written as a person would**, by `FSRelativePath()`:
  relative to the file's folder, climbing out with `..`, when the two
  share a folder from the root - `../../images/msdos330/disk02.img` from
  a VM in `yax86/vm` - and from the root with a leading `/` when they
  share none. In lower case, FAT names being the same in either.

## Managing VMs

- **Manage VMs, from the list, makes, edits, renames, clones and deletes
  VMs**, through `src/vm_folder.c`: a page listing the VMs and then New VM
  (after a separator line if there are any VMs), a
  page for each VM with the images in its drives - two drives a line, A and
  B then HD1 and HD2 - over Edit drives, Rename, Clone and Delete, a name typed
  for Rename and Clone, and a question before Delete. Afterwards it shows
  the list of VMs to manage again with the VM renamed or made chosen - after
  Edit drives, the VM's page - and the list at power on waits for a key, as
  after USB disk mode.
- **A menu box holds at most `kDisplayMenuMaxRows`, 12**, and `DrawItems()`
  leaves out text lines past what fits rather than drawing past the box's
  rows. A VM's page with a line per drive over four items needed 13.
- **A VM's folder is its own, and an image outside it shared.** Rename
  moves the folder with `f_rename()`, at once whatever is in it; Clone
  copies all of it, at any depth; Delete removes all of it, its `vm.cfg`
  last, so that a delete cut short still lists a VM to delete again. An
  image a VM names outside its folder is neither copied nor deleted, and a
  clone names the same one. `vm.cfg` needs no change for either: its paths
  are relative to its own folder, which a sibling has at the same depth.
- **What names a renamed VM or a file in its folder follows it**:
  `boot_default`, and the drives of every VM listed, the moved VM's own
  included, which are written again by `VMConfigSaveDrive()` - so a path
  written from the root comes back relative. A clone's drives naming a file in the
  original's folder are written to name its own copy. Delete leaves other
  VMs' drives naming a file in the folder as they are, to find it gone when
  they start, and its question names those VMs; `boot_default` naming the
  VM is cleared, making the first VM listed the default.
- **A name is up to 8 letters, digits, `-` and `_`**, in capitals as the
  card keeps them: a folder's name, with no long names (`FF_USE_LFN` is 0)
  and no extension. The names of DOS's devices, `CON` and `LPT1` and the
  rest, are refused, since DOS cannot open a folder named so. A name that
  will not do says why on the keys row and leaves it to be typed again; a
  rename to the same name, in any capitals, does nothing.
- **New VM is a form**: the name, typed in place while its row is chosen,
  floppy disks 1 and 2, hard disks 1 and 2, and Create. Floppy disk 1 is
  Empty, New floppy image (360KB), or Existing floppy image, a 360KB image
  from the browser; floppy disk 2 is Disabled or any of those; and a hard
  disk is Disabled, New disk image (10MB), or Existing disk image, a 10MB
  one. A new VM starts with floppy disk 1 empty, floppy disk 2 and hard disk
  2 disabled, and a new disk image as hard disk 1. An existing image is
  shared, like any image outside the VM's folder. A floppy drive with no
  disk is "empty" wherever the menu says what a drive holds, as the status
  rows have it. Backspace on the form only ever erases the name -
  Esc leaves it - so that a press meant for the name does not throw the form
  away. One image in two drives is refused before anything is written.
  Create writes `vm.cfg` with the drives there, as paths relative to it, in
  one `ConfigFileUpdate()` - `VMConfigSaveDrives()` - which removes the line
  of a drive left out, and a line left with nothing else on it.
- **Edit drives is the same form on a VM's drives**, as its `vm.cfg` has
  them, with no name and Save for Create; Save writes the four drives the
  same way, which keeps the file's comments and other lines. An image of the
  VM's own a drive stops holding stays in its folder - nothing is deleted
  behind the user's back - and goes with the VM.
- **A blank disk is zeros, in the VM's folder, unformatted as a new disk
  was**: `fd1.img` or `fd2.img`, 360KB, for DOS's `FORMAT A:` -
  which the floppy controller's Format Track makes possible - and `hd1.img`
  or `hd2.img`, the core's 10MB drive, for FDISK and then `FORMAT C:/S`.
  A name already in the folder - an edit's new disk beside the old - gives
  `hd1_2.img` to `hd1_9.img` and the like instead, so nothing is written
  over. The
  hard disk takes 5.1 seconds to write, a floppy a fifth of a second. Zeros
  rather than space merely allocated, which would hold whatever the card
  held there - an old file's sectors, which FDISK could take for a
  partition table.
- **A clone copies through 32KB of the guest's RAM**, which is not the
  guest's until a VM starts, and is zeroed after. Whole runs of the card's
  blocks go straight between it and the card - see "A transfer is 8KB" -
  and DOS330's 10MB hard disk copies in 11.2 seconds, about 930KB a second.
  Blank disks are written from the same 32KB, zeroed first. The
  progress is drawn as it goes, and nothing else runs: keys and commands
  wait for it to finish, and a host's command sent meanwhile can time out.

## The SD card, the floppy drives and the hard disk

- **Images are on the card's FAT volume**, and guest writes go to the card.
  The card is mounted once, at power on.
- **Floppy disk 2 is there only if `vm.cfg` names it**, and without a VM.
  With two drives DOS has a B: of its own rather than one it shares with A,
  prompting for a disk swap; DOS330 names `fd2=""` for that, and its boot
  is measured with two. `YPlatformConfig.floppy_drives` sets what the
  BIOS's DIP switches report.
- **The menu changes the disk in either drive** for any 360KB image on the
  card, or ejects it. `DiskImageClose()` writes back the sector held and
  closes the file; the FDC then reports the drive not ready, which DOS
  shows as "General Failure ... Abort, Retry, Fail?". The built-in floppy is
  not offered again once ejected - a reset brings it back.
- **An image goes in one drive at a time.** Each drive keeps its own copy of
  a sector, so two drives writing one file would each overwrite the other's
  changes; inserting an image the other drive holds is refused with a
  message.
- **`src/fs.c` is the file system beyond a file's bytes**: paths -
  FatFs's, with no leading slash, compared ignoring case as FAT does, and
  resolved or written relative to a folder - the names the runtime gives
  what it makes, listing a folder, and whole folders: their size, a copy
  and a delete, walking down to four folders deep with one pair of paths
  added to and put back, so a level costs the stack only its lengths.
- `src/file_list.c` lists a folder for the menu's browser: `..`, folders,
  then files exactly the image size, each by name, with hidden and system
  entries left out. It holds 64, and says when a folder had more. Names are
  8.3, as FatFs is built without long names.
- **A hard disk image is the core's 10MB drive**, 306 cylinders, 4 heads, 17 sectors, exactly
  10,653,696 bytes. A blank one is `dd if=/dev/zero of=hd1.img bs=512
  count=20808`, partitioned with FDISK and formatted with `FORMAT C: /S` from
  the boot floppy; New VM and Edit drives make the blank one on the device.
  Its partition starts
  at sector 17, so mtools reach it as `hd1.img@@8704`.
- `src/sdcard.c` runs the card in SPI mode on SPI0, from the SD simplified
  physical layer spec: 400kHz to bring it up, then 24MHz - the 48MHz
  peripheral clock divided evenly, under the 25MHz default speed limit. Each
  command starts from a fresh select that waits for ready. A block alone is
  CMD17 or CMD24, and a run of them one CMD18 or CMD25, ended by CMD12 or the
  stop token. The drives only ever move a block alone, so the boot's card
  accesses are the same either way; FatFs and USB disk mode move runs.
- **The filesystem is FatFs R0.16.** `resources/FatFs/ff16.zip` is the
  release exactly as elm-chan.org serves it, unpacked by `FetchContent` and
  checked against its SHA-256, which says it is unchanged and keeps the tree
  to our own code. To upgrade, replace the zip and hash and carry our
  settings over.
- **`src/ffconf.h` is the release's with four changes**: code page 437,
  `FF_FS_TINY`, `FF_FS_NORTC` and `FF_USE_FASTSEEK`; long names and exFAT are
  off. `configure_file()` copies it over the stock one - `ff.h` includes it
  from its own directory first - and reconfigures when it changes.
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
  it - and the rest most likely flash layout. USB disk mode did it again:
  0.69% slower, 5.1043 seconds against 5.0691, with guest RAM where it was
  and none of its code running in the boot, while everything linked after
  `main.c` and `menu.c` moved 3.5KB along in flash.

## The PSRAM

- **8MB, an ESP-PSRAM64H on one-bit SPI, driven by PIO**:
  `pico/common/psram.c` and `psram.pio`, shared with `pico/bench`, on pio1,
  the LCD having pio0. Reads are Fast Read
  (`0x0B`), writes `0x02`; its ID's second byte, known good die, reads
  `0x5D`. It holds the guest RAM SRAM does not - see "Paging guest RAM".
  The report's `psram` group has its state, the self-test's times and what
  has moved.
- **The serial clock is 62.5MHz**, from 250MHz or 125MHz divided evenly; any
  other system clock leaves it off as `unsupported_clock`. 83MHz and 125MHz
  read back wrong.
- **Each bit is taken one rising edge late**: the board delays the chip's
  output 12-16ns past the falling edge it changes on, which at 62.5MHz is
  after the next rising edge, 8ns on. So a read clocks nine cycles before the
  first bit it keeps - Fast Read's eight dummy cycles and one more - with the
  input synchronizer bypassed, which would add two more cycles. At 31.25MHz
  the bit is valid at its own edge, but only just: one in 128KB flipped.
- **The state machine moves whole words**, header and data, so the output
  shift register is empty between transactions: a part word left over is
  taken for the next header, desynchronizing every transaction after it.
  Memory's little-endian words are swapped on the way through. Words are
  what let the CPU keep up with the wire without DMA - a 512-byte block reads
  in 94us and writes in 83us, against 76us and 74us on the wire. Shifting a
  byte at a time and realigning it in software took 120us.
- **Interrupts are off for each burst.** One taken part way through holds
  chip select low past 8us while the state machine stalls on its FIFO: with
  them on, writing 2MB of noise and reading it back found 66 bad words and
  ran a transfer past its timeout, USB's interrupt the likely one. With them
  off, two runs of 4MB each found none. A burst is about 5us, which is the
  most an interrupt waits.
- **A transfer holds chip select at most 32 bytes**, 4.8us with the command:
  the chip must see it raised within 8us to refresh itself. Bursts are
  aligned to 32 bytes, so none crosses the chip's 1KB pages.
- **Every wait is bounded**, 100us a transfer; a timeout restarts the state
  machine and turns the PSRAM off as `timed_out`. Bringing it up is the
  overclock's pattern besides, with the watchdog armed and a marker in RAM
  startup does not clear, so a hang nothing foresaw starts the next boot
  without it, as `skipped_after_hang`.
- **It costs the boot nothing it runs**: 5.0459 seconds against 5.0644 on
  master, the DOS330 VM, three runs each - flash layout.

## Paging guest RAM

- **With the PSRAM, all 640KB of guest RAM is paged: 128KB of SRAM frames,
  and PSRAM behind them**, through the core's `YPlatformConfig.paged_memory`
  (`core/AGENTS.md`). `pico/common/pager.c` does the paging, and the SRAM
  given to the guest is in `pico/common/yax86_pico.h`, a header per board so
  that a Pico 2 can have its own. Both are shared with `pico/bench`, so that
  it measures what runs here. The pager brings a page into a frame when the
  guest first touches it, from PSRAM, or as zeros if it has never been
  written back, and takes frames back by the clock algorithm, writing a page
  back only if it was written. The report's `paging` group has the counts
  and `fault_us`, the time in the fault handler.
- **Pages are 256 bytes**, measured on the device with `pico/bench`'s
  PicoCalc workloads, the share of processor time in faults:

  | page | seven commands | Word session | Word's pages in |
  | --- | --- | --- | --- |
  | 256 bytes | 0.6% | **1.3%** | 17,502 |
  | 512 bytes | 0.7% | 2.8% | 21,360 |
  | 1KB | 1.0% | 6.9% | 29,380 |

  A fault moves a whole page through the PSRAM at about 0.18us a byte, and
  more of a large page is never used, so fewer bytes moved beats fewer
  faults. Smaller pages cost nothing measurable where nothing faults: the
  DOS330 boot took 6.305, 6.320 and 6.346 seconds. The page table is 2 bytes
  a page, 5KB at 256 bytes; 128 bytes' 10KB is more than the SRAM has spare.
  Traces on a desktop had already ruled out 4KB, which thrashes 128KB of
  frames.
- **No guest RAM is held directly in SRAM.** Holding the first 64KB there,
  under the CPU's direct data window, with the other 64KB as frames, came
  out even on the seven commands with 512-byte pages, 179.0 seconds of
  processor time against 179.7, and more than doubled the Word session's
  share in faults, 2.8% to 6.0%. The programs' code and data are above
  64KB, so it sped up little of what they do, and it left half the frames.
- **Without the PSRAM, or after a crash, the guest has 128KB of SRAM** held
  directly. A hard fault on either core reboots the board through
  `isr_hardfault()`, which replaces the SDK's breakpoint, and leaves a marker
  in RAM startup does not clear; the start after it pages nothing, says so
  on the message row, and reports `off_after_fault`. Paging is the newest
  code a fault could come from, and a board that halts on one needs its
  cable replugged.
- **The runtime reads the BIOS data area through the memory map**,
  `GuestRAMByte()`, since with the PSRAM it is paged.

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

- **Two rows of the 4x10 font at the bottom of the panel**, composed by
  `src/status_bar.c` on core 0 and drawn by core 1, which draws only the
  rows that changed. A line a pixel high in dark grey, two pixels above
  them, sets them off from the guest's screen; the rows between are black.
  They hold what is worth a glance and no more - a status bar that fills
  its space with what rarely matters is one nobody reads.

  ```
  Inserted GAMES.IMG in FD2                                              <- messages and keys
  DOS330         FD1  FD2 ■HD1             CAPS                   ▌▌▌▌▌ 100%  <- the machine
  ```

- **The machine's row, at the bottom edge**: the VM's name; its drives;
  the guest BIOS's lock and insert flags, at `0040:0017` - a PC/XT
  keyboard has no lights - each shown only while it is on; and the battery.
- **A drive is its label alone**, grey after a blank space, and while it is
  in use - for 0.2 seconds after a sector is read or written - a green full
  block before it and the label bright white, as a PC's front panel lights
  a drive. Drives follow each other a space apart, so a light does not run
  into the drive before it, and a drive the machine does not have -
  floppy disk 2 disabled, a hard disk with no image - is left out. The
  images' names are on the status page and in Edit drives: they are mostly
  `fd1.img` and `hd1.img`, and not worth the row.
- **The battery is five cells, a fifth of a charge each**, a cell for each
  fifth begun - 78% is four - in green, yellow below half and red below a
  fifth, then the percentage and `+` while charging. A cell is code page
  437's left half block, which leaves two columns between cells; full
  blocks, and small squares, run together into a bar in the 4x10 font,
  where they fill the cell's width. The battery comes from the keyboard
  controller's register `0x0B`, which the key FIFO poll reads every ten
  seconds in its place. The controller measures it every 20 seconds, and
  answers 0% with no battery, which is shown as no battery.
- **The messages row** shows a message four seconds at a time; the menu's
  keys while it is open, or a problem in their place until the next key;
  and otherwise, while a VM runs, the menu's own key, `Shift+Shift=Menu`,
  dim. Shift is spelled out: the arrows that stood for it read as the up
  arrow key.
- **Keys are labeled as MS-DOS's own programs label them**, `Key=Action`
  pairs two spaces apart - `↑↓=Move  Enter=Select  Esc=Back` - the action
  brighter than the key, so the two read apart in the 4x10 font.
- **Left off**: a warning when the VM falls behind real time, which nobody
  can act on; the guest's time of day, which with no clock to set it from is
  the time since the boot; and the locks that are off.
- **While the list of VMs shows at power on** there is no guest and the
  menu key does nothing, so the machine's row has only the battery.
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
- **The VM is paused while it is open**: no batches run and no frames
  are drawn, and on closing, pacing starts again from the present rather
  than running flat out to catch up. The time is `paused_us` in the report,
  and counts as neither busy nor waited. The guest's clock stands still.
- The guest has seen both Shift keys go down when the menu opens, and would
  not see them come up while the menu has the keyboard, so they are released
  to it then.
- **It is a box drawn over the guest's screen**, white on blue in code page
  437's double lined box, a page at a time: Floppy disk 1, Floppy disk 2 -
  with a second floppy drive - Status, Reset
  and Power off - Esc resumes; a drive's page, with what is in it, Swap disk
  image and Eject, or Choose disk image alone while it is empty; the
  browser; the status page; and a question before a reset or a power off.
  An item that opens another
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
- **Reset and Power off restart the board** with `watchdog_reboot()`, so
  every start begins from the same state, and leave a note for the start
  after it: Reset starts the same VM again at once, without the list, and
  Power off shows the list with that VM highlighted and no countdown,
  whatever `boot_delay` says. A VM that no longer reads leaves the list
  waiting with its problem. What the guest wrote is already on the card.
- **The note is in RAM startup does not clear**, as the overclock's marker
  is, so it survives the reboot but not a power cycle. The start after clears
  it before acting on it, so a later reboot - picotool's, or
  the power button's - starts as from power on. Power off is offered only
  with a VM, since without a card there is no list: Reset then reboots to
  the built-in floppy with no note.
- **A test drives it as a hand would**, with `key shift press`, `key rshift
  press` and their releases, and checks it with `screenshot`; the report's
  `menu` group says whether it is open and which page shows.

## The USB device

- **The PicoCalc is one USB device with three functions**, described in
  `src/usb_descriptors.c`: the serial port (CDC, interfaces 0 and 1), which
  `pico_stdio_usb` drives; picotool's reset interface (2), from
  `pico_usb_reset`; and a card reader (3), mass storage, in
  `src/usb_disk.c`. The serial port and the reset interface are described
  as `pico_stdio_usb` describes them, with the SDK's vendor and product
  IDs, so a host finds `/dev/ttyACM*` and `picotool reboot -f` works as
  before.
- **A third function means describing the device here**, which
  `pico_stdio_usb` allows only once the application links `tinyusb_device`
  itself - and then it turns off three things it otherwise does: starting
  TinyUSB, running `tud_task()` from a low priority interrupt, and the reset
  interface. `CMakeLists.txt` turns each back on. **Without the first two
  the board never appears on USB**, so neither picotool nor the 1200 baud
  reset can reach it, and only BOOTSEL - the Pico's button held while the
  cable goes in - brings it back. Before flashing a change to any of this,
  check the image still calls `tusb_rhport_init` from `stdio_usb_init`.
- `src/tusb_config.h` configures TinyUSB, and is found ahead of the one
  `pico_stdio_usb` carries, which configures nothing for an application
  that links TinyUSB.
- **The card reader is always there, and empty until the list at power on
  shares the card**: TEST UNIT READY answers NOT READY, MEDIUM NOT PRESENT,
  so a host sees a removable disk with no media - `sdX` on Linux, 0B. Being
  there from power on is what lets it take a card later without the device
  going away, which would take the serial port and the reset interface with
  it. Linux asks every two seconds, and sees a card shared within that.
- **USB disk mode is reached only from the list at power on**, never while a
  VM runs: nothing has the card open there, and no guest holds a copy of a
  disk's FAT that a computer could change under it. Its row, USB disk mode,
  opens a page with Whole SD card, which unmounts the card's volume -
  `DiskImageUnmountCard()` - and shares it, and Disk image, which opens the
  browser on `yax86` listing floppy and hard disk images alike, each marked
  which, and shares the one chosen.
- **An image on USB is its own disk**: a floppy is a FAT12 volume with no
  partition table, which a computer mounts whole, and a hard disk has its
  table, with the partition at sector 17. The card stays mounted, and the
  image is read and written through `DiskImage` a run of sectors at a time,
  `DiskImageReadSectors()` and `DiskImageWriteSectors()`, which FatFs hands
  to the card whole as far as a cluster goes, found through the fast seek's
  map - and closed on disconnecting. Through the drives' byte at a time
  instead, each sector written was read first, and writing ran at 137KB/s.
  The browser shows no "in FD1": at power on the drives have no images.
  The card's page shows whether a computer has it and what has moved;
  Disconnect asks first if the computer has the card and has not ejected it,
  then mounts the card again and reads the VMs and settings afresh, showing
  the list without counting down. The report's `usb` group has the same.
- **The computer reads and writes the card's own blocks**, its partition
  table and FAT volume, in the USB callbacks, which pico_stdio_usb's task runs
  from a low priority interrupt. They cannot leave the card to the main loop:
  TinyUSB's task drains its queue before it returns, and a callback that
  answers "not yet" is queued again at once, so the interrupt would spin and
  the main loop never run. Nothing else touches the card while it is shared,
  and the main loop only flips whether it is, which the interrupt, preempting
  it, always sees whole.
- **A transfer is 8KB, sixteen blocks, a callback**
  (`CFG_TUD_MSC_EP_BUFSIZE`), each one CMD18 or CMD25. Writing 8MB to a file
  on the card ran at 609KB/s and reading it 720KB/s; to an image, 614KB/s
  and 688KB/s. A block a command, it was 246 and 605KB/s, and an image 137
  and 492KB/s.

  | buffer | card read | card write | image read | image write |
  | --- | --- | --- | --- | --- |
  | 4KB | 711 | 568 | 682 | 572 |
  | 8KB | 720 | 609 | 688 | 614 |
  | 16KB | 721 | 636 | 691 | 644 |

  16KB gains writes 4.5% more for another 8KB of RAM.
- **USB is what is left**: `read_us` and `write_us` in the report are the
  time on the card, against the computer's whole transfer. Reading 48MB the
  card took 19.7 of 58 seconds, about 2.4MB/s against 24MHz's 3MB/s, and
  USB, at full speed, the rest, about 1.05MB/s. The two take turns - TinyUSB
  asks for the next transfer only once the last is sent, and 0.18 has no
  asynchronous MSC calls - so overlapping them, worth up to about 1MB/s
  each way, would mean running TinyUSB's task some other way.
- Each write has reached the card before its callback returns, so
  SYNCHRONIZE CACHE has nothing to do.
- **The card's size** is read from its CSD register (CMD9) when it is
  brought up - `SdcardGetBlockCount()`, `sd_blocks` in the report - as the
  reader has to say how many blocks it holds. FatFs never needed it.

## Commands over USB

- **A host drives the runtime with commands on the same serial port**, a
  logfmt line each - `id=7 cmd=screen` - answered by `kind=reply` lines with
  that id, the last with `status=ok`, or `status=error` and an `error`.
  `src/command.c` reads them and `main.c` carries them out:

  | command | does |
  | --- | --- |
  | `ping` | answers, and nothing else |
  | `report` | a line for each group of the status - `system`, `boot`, `display`, `fd1`, `fd2`, `hd1`, `hd2`, `vm`, `usb`, `keyboard`, `speaker`, `menu`, `pacing` |
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

## The 4x10 fonts

- 80 columns in 320 pixels is four pixels a character, and a 640-pixel text
  screen shrunk to fit is unreadable. What works is a font drawn for the
  cell - C64 80-column software used 4x8.
- `src/font_4x10.c` holds four complete code page 437 fonts of that size,
  chosen by `font` in `yax86/yax86.cfg` or from the list at power on (`Font`),
  for both the guest's text modes and the menu and status rows:
  - `ymini` (Ymini, the default): 3+1 glyphs (three columns of ink and a
    blank fourth column) drawn for this cell, with 7-row capitals and digits.
  - `atari800` (Atari800): 3+1 glyphs with 6-row capitals, from Atari800's
    4x8 font.
  - `miniwi` (Miniwi-8): 3+1 glyphs with 5-row capitals, from miniwi-8.
  - `creep` (Creep): 4-column glyphs, rugosi's ASCII range from
    picocalc_runCPM and Serg Podtynnyi's rest of code page 437.

  Where each font's glyphs come from, and under which license, is at the top
  of `src/font_4x10.c`.
- Every glyph outside the box drawing range sits one row lower than the box
  drawing ones, leaving a blank row between lines of text while box drawing
  fills the cell and joins. The shift is in the table, so each is a plain
  font. The three glyphs using the bottom row - `0x02`, `0x0F`, `0x16`,
  cell-filling patterns - are not shifted.
