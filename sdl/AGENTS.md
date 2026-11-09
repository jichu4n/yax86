# sdl - the SDL runtime

An SDL3 runtime for the emulator, built natively and through Emscripten as
`yax86_sdl.{wasm,js}`. Debugging the browser build is in the root
`AGENTS.md`.

- **How much guest time a pass of the main loop runs comes from the wall
  clock**, not from how often it is called. Under Emscripten
  `requestAnimationFrame` drives it at the display's rate or, with no vsync
  to lock to, faster - 120-200Hz on an i7-3667U, which with a fixed budget
  ran the 8088 and its clock at twice speed. A gap over `MAX_CATCH_UP_NS` is
  dropped, so a tab returning from the background does not burst.
- **The loop steps no more often than `MIN_STEP_NS`**, a little under 1/60s.
  The display refreshes at 60Hz, so more steps draw nothing new, and with
  idle skipping a pass costs about one trip round the guest's idle loop, so
  an idle machine's cost tracks the callback rate. It sits *under* 1/60s
  because at exactly 1/60s a slightly early callback would halve the rate.
  Events are pumped on every callback.
- An optional floppy image path, default `floppy_a.img`. **A 10MB hard disk
  is attached by default** - blank unless `--hdd <image>` names one, to be
  partitioned and formatted from DOS - and `--no-hdd` leaves it out. The
  default matters because the browser build has no command line.
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
