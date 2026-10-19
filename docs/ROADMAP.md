# Roadmap

Status is updated as milestones land. Later milestones are not in progress.

## Milestone 1 — Project foundation (implemented)

- CMake build with pinned dependencies
- Logging
- Versioned JSON configuration
- SDL3 window and Windows XP-inspired shell
- Catch2 tests
- Windows CI

## Milestone 2 — Minimal playable NES (implemented)

- iNES and NES 2.0 cartridge parsing
- NROM (mapper 0) only; other mappers are rejected with an error
- Official 6502 opcodes, with decimal mode ignored the way the NES does
- CPU bus, controller ports, and OAM DMA
- Initial PPU: registers, nametables, palettes, background, sprites, and vblank NMI
- Minimal APU: pulse, triangle, and noise mixed to the host audio device
- Keyboard controls, pause, reset, and a nearest-filtered 256×240 picture
- Iris Demo, an original NROM program generated next to the executable

NTSC frame timing in that version was paced on the UI thread. Mappers other than NROM, DMC, and PAL waited for Milestone 3.

## Milestone 3 — Accuracy and mapper expansion (implemented)

- PPU A12 is tracked on each dot so a mapper can count scanlines. Pixels are still drawn once per scanline.
- APU frame counter with its IRQ, plus DMC playback. A DMC fetch stalls the CPU for four cycles.
- OAM DMA and interrupt lines stay in step with the CPU.
- MMC1, UxROM, CNROM, and MMC3. Other mappers are rejected and leave the previous game loaded.
- Original conformance tests for banking, frame IRQ, DMC, and MMC3 IRQ.
- NTSC timing. A NES 2.0 header can select PAL clocks and 312 scanlines. Dendy is recognized with its own clock and scanline count.

Pattern bytes were not fetched dot by dot in that version. That fetch, and the mappers after MMC3, landed in Milestone 11.

## Milestone 4 — Save systems (implemented)

- Versioned `IRS1` state files. A file names the game by content hash, mapper, and region, and stores CPU, PPU, APU, mapper, cartridge RAM, controllers, bus, and timing.
- Ten slots. F5 saves the selected slot, F7 loads it, and Shift+1 through Shift+0 select slots 1–10. The State Slots window shows a thumbnail, title, time, and description.
- A damaged file, or a file for a different game, is refused and the running game stays as it was.
- Battery-backed RAM is a separate `.sav` file, loaded with the game and written when the game closes or the program exits. Reset does not erase it.

## Milestone 5 — Modern graphics (implemented)

- Integer scales from 1× through 6×, and fit. A requested scale that does not fit steps down, and the status bar says so.
- Pixel aspect is the default. 4:3 is the television override. Neither mode stretches one axis to fill the window.
- Borderless fullscreen from the View menu or Alt+Enter. VSync can be turned off.
- Original, Smooth, Sharp, and CRT presets. Brightness, contrast, saturation, and gamma are a later color stage.
- The presentation shader samples a copy of the PPU texture. Save thumbnails still use the unfiltered picture. If the shader cannot be created, the window shows crisp pixels.
- Performance can show host fps, emulated fps, and the time spent in emulation and presentation.

The game library, audio device settings, and other consoles were still ahead at that point. The library and audio settings landed in Milestone 6. Other consoles wait.

## Milestone 6 — Complete desktop frontend (implemented)

- The game library is `library/library.json`, separate from configuration. It lists recent games, can search and sort, and can scan a folder and its subfolders. Removing an entry leaves the ROM on disk. A missing file is marked when the list is shown.
- Settings changes the theme, log level, library sidebar, picture, volume, mute, audio device, latency, and keyboard bindings. Version 1 and version 2 configuration files still load.
- If the playback device is disconnected, the window opens the default device. Volume scales a copy of the samples that are queued. The APU buffer is left as the emulator wrote it.
- Help lists the pinned open-source libraries. A CPack ZIP contains the executable, Iris Demo, the readme, and the license files.

Release hardening and other consoles were still ahead at that point. Release hardening landed in Milestone 7. Other consoles wait.

## Milestone 7 — Compatibility and release hardening (implemented)

- The original conformance programs stay in the suite: official CPU behavior, UxROM, CNROM, MMC1, MMC3 IRQ, the frame counter, DMC, and PAL and Dendy frame length. Results and limits are in `docs/COMPATIBILITY.md`.
- The original demo's picture after 90 idle frames is pinned. A second run must match, and holding Right must change the picture. The same suite rejects a debug build that averages more than 100 ms per frame.
- `iris --profile rom.nes` runs 300 frames without a window and prints the target, average, and slowest frame. A Release build of that command is part of Windows CI.
- An unexpected C++ exception or Windows hardware exception is written to `logs/crash.log` and shown in a dialog. Configuration is not saved after that failure.
- Version 0.7.0 is the unsigned Windows release candidate. The executable carries a version resource. The Release build is produced with the existing `windows-msvc-release` preset.

Linux and macOS desktop builds, and the shared core, landed in Milestone 8. Full mobile frontends and other consoles wait.

## Milestone 8 — Portability validation (implemented)

- `src/core/iris.h` is the C API. It creates the NES system, loads a ROM, runs a frame, and copies a save state. The header does not include SDL, ImGui, or CPU, PPU, or mapper types. A Windows test loads Iris Demo through that API and checks the pinned picture hash.
- `IRIS_DESKTOP` builds the SDL window. It is forced off for Android and iOS, and those configures do not fetch SDL or Dear ImGui.
- Linux uses the `linux-debug` preset. macOS uses `macos-debug` and requests an OpenGL 3.2 compatibility context, because macOS does not provide OpenGL 3.3 Core. The picture shaders stay GLSL 330; if they fail, the window shows crisp pixels.
- User data on Linux and macOS is `$XDG_DATA_HOME/IrisEmulator`, or `$HOME/.local/share/IrisEmulator`. `IRIS_DATA_DIR` still wins.
- `ports/android` is a JNI library and a Java class with native declarations. It is not an app. `ports/ios/iris_bridge.h` is the same C API for a future Swift or Objective-C caller. There is no touch control, audio-focus handling, Gradle screen, or Xcode app.
- GitHub Actions builds the desktop on Ubuntu and macOS. This repository's development machine is Windows, so those two jobs are the platform check. The macOS build is not signed or notarized.

The first portability run failed to configure on Ubuntu and failed to compile on macOS. Milestone 9 repaired those jobs. Full mobile frontends and other consoles wait.

## Milestone 9 — Portability CI repair (implemented)

- Ubuntu installs `libxtst-dev`. SDL 3.4.18 is built from source, and its X11 check requires XTEST. `SDL_X11_XTEST` stays on.
- Picture-pass OpenGL pointers use `APIENTRY` from `SDL_opengl.h`. That is the Win32 calling convention on Windows and an empty macro for GCC and Apple Clang. `-Werror` stays on.
- GCC also rejected a move of a const cartridge result, a Windows-style path treated as absolute on POSIX, and a save-slot name that could not fit the worst `snprintf` expansion. Those are host-build fixes. NES timing, mappers, and the picture hash are unchanged.
- Version 0.9.0. On commit `0a1547c`, Windows run [38257147101](https://github.com/anildhan123/Project-Iris-NES-Emulator/actions/runs/38257147101) passed debug and release. Portability run [38257147297](https://github.com/anildhan123/Project-Iris-NES-Emulator/actions/runs/38257147297) passed Ubuntu debug and macOS debug: configure, build, `ctest`, and `iris --version`. Those jobs do not open a window. A green compile is not a visible desktop on Linux or macOS.

Full mobile frontends and other consoles are not started.

## Milestone 10 — Classic desktop shell (implemented)

- The window draws its own frame: a shaded title bar, caption buttons, a border, a menu, and a toolbar. Minimize, maximize, restore, and close work. Drag the title to move the window, and drag the border to resize it. Fullscreen hides that frame.
- The toolbar buttons are Open, Pause or Resume, Reset, Save, and Load. They call the same actions as the menus.
- Themes are Blue, Olive, and Silver. Settings and the configuration file use `blue`, `olive`, and `silver`. A file that still contains `xp-blue`, `xp-olive`, or `xp-silver` loads, and the next save writes the current name.
- Version 0.10.0. The picture hash, mappers, and timing are unchanged.

Full mobile frontends and other consoles are not started.

## Milestone 11 — Play, input, and picture timing (implemented)

- Each visible dot fetches the background pattern for its tile and draws that pixel. Sprite patterns are read on dots 257–320 for the following line.
- Unofficial 6502 opcodes run. A jam opcode stops the CPU.
- AxROM, Color Dreams, BNROM, GxROM, and Camerica load beside the earlier mappers.
- Options, Controllers chooses Keyboard, None, or a connected gamepad for port 1 and port 2, and maps that pad's buttons. The keyboard keys stay in Settings.
- Emulation, Frame Step runs one frame and leaves the game paused. Fast Forward runs frames without waiting out the frame period, and it does not queue that audio.
- Emulation runs on a worker thread. The window copies the finished picture and samples.
- View, Screenshot writes an unfiltered bitmap into the screenshots folder. Overscan crops eight pixels from each edge of that copy. The PPU buffer stays 256×240.
- Version 0.11.0. Configuration version 4 adds the controller ports and overscan. Versions 1, 2, and 3 still load.

Full mobile frontends and other consoles are not started.

## Milestone 12 — Region and power cycle (implemented)

- Emulation, Region chooses Automatic, NTSC, PAL, or Dendy. Automatic follows the cartridge header. The other three override it and change the frame length without clearing work RAM.
- Emulation, Power Cycle clears work RAM and restarts the CPU. Battery-backed cartridge RAM stays. Reset still keeps work RAM.
- Emulation, Game Information shows the title, mapper, region, battery, content id, and file path.
- Version 0.12.0. Configuration version 5 adds `emulation.region`. Versions 1, 2, 3, and 4 still load.

Full mobile frontends and other consoles are not started.

## Milestone 13 — Turbo (implemented)

- Settings maps Turbo A and Turbo B. The defaults are A and S. A gamepad uses the right shoulder and the left shoulder.
- Turbo is on for one emulated frame and off for the next. Holding the normal button at the same time keeps that button down.
- Version 0.13.0. Configuration version 6 adds the turbo keys. Versions 1 through 5 still load.

Full mobile frontends and other consoles are not started.

## Milestone 14 — Settings for region, overscan, and shortcuts (implemented)

- Settings, Emulation chooses Automatic, NTSC, PAL, or Dendy. Apply is what changes the running system. The Emulation menu still changes it immediately.
- Settings, Video includes overscan. The View menu still toggles it immediately.
- Settings maps Fast Forward and Screenshot. The defaults are F9 and F12. Those keys are shown on the menus.
- Settings, Paths shows the screenshots folder beside the other data folders.
- Version 0.14.0. Configuration version 7 adds the fast-forward and screenshot keys. Versions 1 through 6 still load.

Full mobile frontends and other consoles are not started.

## Milestone 15 — Super NES LoROM (implemented)

- The startup screen offers NES and Super NES. Each card can continue into the window or play Iris Demo. A `.sfc` or `.smc` path selects Super NES. The NES core is unchanged.
- Super NES loads LoROM and HiROM, with or without a 512-byte copier header. Other mappings are refused.
- The CPU runs in emulation mode or native mode. REP can widen the accumulator to 16 bits. XCE switches modes. NMI is raised at vblank. The 65816 instruction set runs. STP stops the CPU.
- Background modes 0 through 7 and 8×8 sprites are drawn into a 256×224 picture. Mode 2 does not apply its offset layer, and modes 5 and 6 stay 256 pixels wide. General DMA and direct or indirect HDMA can write video registers. Window 1 can mask a background or sprite, and color math can add or subtract a fixed color. The CPU runs the 65816 instruction set. A game can wait on vblank, use the multiply and divide registers, and keep LoROM or HiROM save RAM. Controller port 1 is live. The sound chip is silent. Iris Demo draws stripes and a marker moved with the direction keys. Enhancement chips and Super NES save states are not in this build.
- The startup cards use original console drawings. Battery mode uses the same process and thread opt-out as NES, and the Super NES interpreter is optimized in debug builds so a frame still finishes on battery.
- Version 0.15.0. The configuration schema stays at version 7.

Full mobile frontends and other consoles are not started.

## Milestone 16 — Super NES picture layers (implemented)

- Background 2 draws behind background 1 in modes 0 through 5. Mosaic can repeat a background pixel across a block of 2 to 16 pixels. Sprites use the eight size pairs, from 8×8 up to 64×64.
- Mode 2 still does not apply its offset layer. Modes 5 and 6 stay 256 pixels wide. Backgrounds 3 and 4, subscreen color math, and tile priority are not in this build.
- The sound chip, enhancement chips, and Super NES save states are not in this build.
- Version 0.16.0. The configuration schema stays at version 7.

Full mobile frontends and other consoles are not started.

## Milestone 17 — Super NES picture completion (implemented)

- Background map and character addresses follow the cartridge's video memory steps. All four backgrounds scroll. Backgrounds 3 and 4 draw in the modes that have them. A tile's priority bit, and a sprite's priority, decide which pixel is in front. Modes 2, 4, and 6 apply the per-column offset from background 3. Color math can add the subscreen instead of the fixed color.
- Modes 5 and 6 stay 256 pixels wide. The sound chip, enhancement chips, and Super NES save states are not in this build.
- Version 0.17.0. The configuration schema stays at version 7.

Full mobile frontends and other consoles are not started.

## Milestone 18 — Super NES sound (implemented)

- The SPC700 runs a program the cartridge uploads through ports `$2140`–`$2143`. The Nintendo IPL ROM is not in this build. A jump to `$FFC0` returns to that upload protocol.
- The DSP plays BRR samples, with direct gain, a simplified envelope, noise, and pitch modulation. Echo uses a buffer inside the DSP, not APU RAM. Samples are not Gaussian-interpolated. The mix is mono and resampled to 48000 Hz for the host.
- Iris Demo does not upload a sound program, so it stays silent. A cartridge that needs an enhancement chip, or scanline-accurate HDMA, can still fail. Modes 5 and 6 stay 256 pixels wide. Super NES save states are not in this build.
- Version 0.18.0. The configuration schema stays at version 7.

Full mobile frontends and other consoles are not started.

## Milestone 19 — Super NES enhancement chips (implemented)

- A DSP-1 board answers multiply, inverse, triangle, and rotate on its data port. Other DSP-1 commands return no words. DSP-2, DSP-3, and DSP-4 are not in this build. Inverse and the angle commands use host arithmetic, not a coefficient ROM.
- A Super FX board runs a GSU program from Game Pak RAM. The program can load and store bytes and words, add, branch, and plot an 8-bit pixel. The pixel cache, packed 2-bit and 4-bit screens, and the opcodes this build does not include are absent. The GSU runs when GO is set, up to a budget, rather than on the scanline.
- An SA-1 board runs a second 65816 from the vector at `$2203` when `$2200` bit 5 is set. I-RAM and BW-RAM are visible to both CPUs. Bitmap mode, character conversion, SA-1 DMA, and variable-length bit data are not in this build.
- A chip this build does not include still will not play. ExHiROM is still refused. Modes 5 and 6 stay 256 pixels wide. Super NES save states are not in this build.
- Version 0.19.0. The configuration schema stays at version 7.

Full mobile frontends and other consoles are not started.

## Milestone 20 — Super NES cycle timing (implemented)

- A DMA started by `$420B` costs 8 cycles plus 8 per byte, in the same unit as a CPU instruction, and that cost is charged when the channel starts. A transfer larger than the rest of the frame delays the following instruction. HDMA is initialized at the end of the first visible scanline from the current `$420C`, then each visible line transfers before it is drawn. Init, transfer, and reload each cost 8 cycles per channel.
- A vertical IRQ fires once on the line in `$4209`. A horizontal IRQ fires once per line when the line counter reaches the dot in `$4207`, counted as four cycle units per dot. Both together fire once. The picture of a frame is drawn before that frame's vblank, so a sprite update in the NMI handler appears on the next frame.
- This is not dot-accurate inside an instruction. There is no DRAM refresh and no short scanline. Modes 5 and 6 stay 256 pixels wide. ExHiROM, DSP-2, DSP-3, DSP-4, the rest of each enhancement chip, and Super NES save states are not in this build. The Nintendo IPL ROM is not included.
- Version 0.20.0. The configuration schema stays at version 7.

Full mobile frontends and other consoles are not started.

## Milestone 21 — Super NES ExHiROM (implemented)

- An ExHiROM board loads. Banks `$C0`–`$FF` read the first 4 MB, and banks `$40`–`$7D` plus the system-bank mirror read the second 4 MB. A header past 4 MB is the one the CPU sees.
- Banks `$7E` and `$7F` stay work RAM. Banks `$FE` and `$FF` stay ROM on LoROM, HiROM, and ExHiROM, so a write to work RAM does not show up in those ROM banks.
- DSP-2, DSP-3, DSP-4, and the other enhancement chips are still refused. Modes 5 and 6 stay 256 pixels wide. Timing inside an instruction is still approximate. Super NES save states are not in this build. The Nintendo IPL ROM is not included. A cartridge this build does not cover still will not play.
- Version 0.21.0. The configuration schema stays at version 7.

Full mobile frontends and other consoles are not started.

## Milestone 22 — Super NES save states (implemented)

- A Super NES state uses the `IRSN` envelope. It stores both 65816s, work RAM, video RAM, the picture, the SPC700 and DSP, the DSP-1 port, and the Super FX and SA-1 memory that the loaded board owns. It does not store the ROM. A file for a different game is refused, and a damaged file leaves the running machine as it was.
- File → System switches between NES and Super NES after one system is already open. Opening a `.nes` file from Super NES, or a `.sfc` or `.smc` file from NES, switches to the system that file belongs to.
- DSP-2, DSP-3, DSP-4, and the other enhancement chips are still refused. Modes 5 and 6 stay 256 pixels wide. Timing inside an instruction is still approximate. The Nintendo IPL ROM is not included. A cartridge this build does not cover still will not play, and a commercial soundtrack is not claimed.
- Version 0.22.0. The configuration schema stays at version 7.

Full mobile frontends and other consoles are not started.

## Compatibility follow-up after Milestone 22 (implemented)

- `$4212` reports horizontal blank. Modes 5 and 6 draw 512 dots. Each scanline charges DRAM refresh, and NTSC shortens one scanline.
- DSP-1 answers more geometry commands. DSP-2, DSP-3, and DSP-4 boards load. Super FX plots packed 2-bit and 4-bit screens and runs additional opcodes. SA-1 can DMA between I-RAM and BW-RAM.
- NES MMC2 and NINA-003 load beside the earlier mappers.
- The Nintendo IPL ROM is still not included. Timing inside an instruction is still approximate. A commercial soundtrack is not claimed. Other NES mappers and the remaining chip command sets stay out until a tested program needs them.

Full mobile frontends and other consoles are not started.

## Firmware, chips, mappers, and soundtrack follow-up (implemented)

- A user may place a 64-byte IPL image at `firmware/ipl.rom`. When present it is mapped at `$FFC0` and executed. The built-in upload protocol remains the default. Nintendo's IPL dump is not shipped.
- DSP-1 projection and attitude helpers, DSP-2 convert replies, DSP-3 and DSP-4 command slices, and more Super FX ALU ops are in this build.
- NES MMC4, VRC1, mapper 87, DxROM, and BF9096 load beside the earlier boards.
- The DSP mixes stereo at 48000 Hz with Gaussian BRR interpolation. A cartridge that uploads an SPC700 soundtrack plays through that path. Timing inside an instruction is still approximate.

Full mobile frontends and other consoles are not started.
