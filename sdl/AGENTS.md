# sdl - the SDL runtime

An SDL3 runtime for the emulator, built natively and through Emscripten as
`yax86_sdl.{wasm,js}`. Debugging the browser build is in the root
`AGENTS.md`.

- **How much guest time a pass of the main loop runs comes from the wall
  clock**, not from how often it is called, scaled by
  `YPlatformGetCPUCyclesPerSecond(&g_platform)` so guest real time matches wall
  time at every CPU clock ratio. Under Emscripten
  `requestAnimationFrame` drives it at the display's rate or, with no vsync
  to lock to, faster - 120-200Hz on an i7-3667U, which with a fixed budget
  ran the 8088 and its clock at twice speed. A gap over `MAX_CATCH_UP_NS` is
  dropped, so a tab returning from the background does not burst.
- **`--speed 1x|2x|4x|8x|16x|max` sets the CPU clock**, default `1x`, with
  guest time still locked to wall time: fixed presets pick a power-of-two
  multiple of 4.77MHz (keeping `YPlatformSync()` on its power-of-two shift
  path), and `max` adapts `cycles_per_pit_tick` from recent non-idle throughput
  (ignoring idle-skipped batches via `did_skip_idle` and `HLT`/bulk-`REP`
  batches), capped at `Y_PLATFORM_MAX_CYCLES_PER_PIT_TICK` (64x 4.77MHz,
  ~305.5MHz). The browser build has no command line and stays at `1x`.
- **The loop steps no more often than `MIN_STEP_NS`**, a little under 1/60s.
  The display refreshes at 60Hz, so more steps draw nothing new, and with
  idle skipping a pass costs about one trip round the guest's idle loop, so
  an idle machine's cost tracks the callback rate. It sits *under* 1/60s
  because at exactly 1/60s a slightly early callback would halve the rate.
  Events are pumped on every callback.
- An optional floppy image path, default `floppy_a.img`, 360KB (368,640 bytes),
  720KB (737,280 bytes), 1.2MB (1,228,800 bytes) or 1.44MB (1,474,560 bytes)
  from `YFDCFindFormat()`, and `--fd-type 360k|720k|1.2m|1.44m` to set drive
  0's mechanism type (`YPlatformConfig.floppy_drive_types[0]`, checked against
  the image with `YFDCDoesDriveTypeSupportFormat()`); without `--fd-type` the
  drive is a `360KB` drive (`Y_FDC_DRIVE_TYPE_360KB`, so a 720KB image needs
  `--fd-type 720k`), and the browser build's packaged boot floppy remains
  360KB. **A 10MB hard disk is attached by default** - blank unless
  `--hdd <image>` names one, to be partitioned and formatted from DOS - and
  `--no-hdd` leaves it out. The default matters because the browser build has
  no command line.
- `--cga` or `--mda`, default CGA, through `YPlatformConfig.video_adapter`
  before `YPlatformInit()`. It decides what the platform registers and what
  the PPI's DIP switches report, which the BIOS reads.
- Both disks are loaded into memory and guest writes are discarded: FDISK,
  FORMAT and booting from C: work within a session, and every run starts the
  same.
- **`src/audio.c` turns the speaker's frequency into a square wave.** The
  core reports a frequency, not samples, so nothing reconciles emulated time
  with the audio clock - the synthesizer holds a tone until told otherwise.
  - Amplitude ramps over a millisecond at each start and stop: cutting a
    square wave at full amplitude is louder than a real speaker's click.
  - Browsers start audio only after interaction, so `AudioResume()` runs
    again on the first input event; the POST beep is never heard there.
  - To check audio without listening, use SDL's disk driver:
    `SDL_AUDIO_DRIVER=disk SDL_AUDIO_DISK_OUTPUT_FILE=out.raw`. SDL may upmix
    to interleaved stereo.
- **Logs go to stdout**, which Emscripten routes to the browser console, as
  the logfmt lines in `core/AGENTS.md`. Errors only by default; `min_level`
  in `main()` raises that. The runtime's own lines are module `sdl`, through
  a `YLogger` of its own sharing the platform's configuration, so it can log
  before the platform is up.
