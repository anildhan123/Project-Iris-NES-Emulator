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
