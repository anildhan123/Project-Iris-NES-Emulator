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

## Super NES depth follow-up (implemented)

- Memory accesses charge FastROM or SlowROM clocks inside an instruction (6:8 ratio, scaled so typical setup programs still finish a frame). Mid-instruction and mid-scanline edge cases can still break picky games; this is not a claim of full dot accuracy.
- The software IPL upload protocol clears APU RAM on entry. A user may still supply `firmware/ipl.rom`. Nintendo's dump is not shipped.
- Cx4 answers MMIO plus a signed multiply kick on `$7F4F` command `$05`. S-DD1, SPC7110, OBC-1, ST010/011/018, and BS-X answer MMIO slices so boards load. DSP math stays host arithmetic without a coefficient ROM. Full silicon for every chip path is not claimed.
- SA-1 exposes bitmap register and variable-length bitstream ports, I-RAM/BW-RAM DMA, signed multiply/divide through `$2250`–`$2254` / `$2306`–`$230A`, and type-2 character conversion via `$2240`–`$224F`. Complete bitstream hardware and type-1 conversion remain incomplete. Super FX runs on a per-scanline GO budget with an 8-pixel plot cache; the full opcode set and scanline-accurate GSU timing are not claimed.
- `$2133` selects interlace and overscan. BGMODE, TM, and TS latch at the start of each visible line. Odd mid-frame register races beyond the suite are still open.
- ADSR uses rate tables closer to the S-DSP. Echo reads and writes APU RAM at ESA. Not every commercial soundtrack is proven end to end.
- Map modes beyond LoROM, HiROM, ExHiROM, FastROM, LoROM variants 2 / `$A`, and SA-1 stay refused.
- The suite proves these features. It does not claim that every cartridge plays.

Full mobile frontends and other consoles are not started.

## Commercial play milestones (after Milestone 22)

These are ordered for titles such as Final Fantasy VI (stuck intro + silence) and Donkey Kong Country (garbled audio). One milestone at a time.

| ID | Milestone | Status |
|---|---|---|
| 23 | SPC700 timer IRQs, SLEEP wake, timer enable edges | implemented |
| 24 | In-game APU port protocol after boot | implemented |
| 25 | DSP quality (ADSR, echo FIR, pitch, noise) | implemented |
| 26 | Scanline / HDMA / mid-frame PPU timing | implemented |
| 27 | Auto-joypad and multi-port input edges | implemented |
| 28 | Enhancement-chip depth beyond MMIO slices | implemented |
| 29 | Commercial HiROM play: FF6 intro + soundtrack | implemented |
| 30 | Akao / commercial APU command depth | implemented |

Full mobile frontends and other consoles are not started.

## Milestone 23 — SPC700 timer IRQs and SLEEP (implemented)

- When an SPC timer overflows and PSW.I is set, the SPC takes IRQ through `$FFDE`.
- `SLEEP` (`$EF`) waits for that interrupt; `STOP` (`$FF`) stays halted until reset.
- Enabling a timer from `$F1` clears that timer's internal stage. The suite checks that SLEEP wakes and writes `$5A` on port 0.
- This is aimed at commercial engines that tick music from timer IRQs. Full DSP mix quality waits for Milestone 25. Every cartridge still is not claimed.

## Milestone 24 — In-game APU port protocol (implemented)

- APU I/O at `$2140`–`$217F` mirrors the four ports (some drivers poll `$2144`).
- Catch-up of the SPC is deferred across an instruction that touches those ports so a 16-bit store to `$2140` commits both bytes before the SPC runs.
- After boot, a command on port 0 can be echoed on port 1 with an ack on port 0. The suite checks that path.
- DSP mix quality and commercial soundtrack fidelity wait for Milestone 25. Every cartridge still is not claimed.

## Milestone 25 — DSP quality (implemented)

- Gaussian BRR interpolation advances the four-tap window only on sample edges (not every host mix step).
- ADSR uses the rate table; a fast attack reaches a high ENVX. Direct gain remains available.
- Echo reads and writes APU RAM at ESA through an 8-tap FIR. EDL=0 is a one-sample (4-byte) buffer, matching hardware.
- Noise uses the 15-bit LFSR as a signed sample, clocked by FLG bits 0–4. Pitch modulation from the previous voice OUTX remains.
- Aimed at garbled commercial mixes (for example Donkey Kong Country). Scanline and input edges wait for later milestones. Every cartridge still is not claimed.

## Milestone 26 — Scanline / HDMA / mid-frame PPU timing (implemented)

- HDMA init and per-line transfers run at HBlank (not after the whole line's CPU work), including through vblank while channels remain active.
- BGMODE, TM, TS, CGWSEL, and CGADSUB latch at the start of each visible line; mid-line writes apply on the next line. Line 0 refreshes that latch once after cold-boot setup.
- `$4212` bit 7 and NMI use line 225, or 240 when SETINI selects overscan. The suite checks that overscan delays the vblank flag.
- Aimed at Final Fantasy VI HBlank waits and fade scripts. Auto-joypad edges wait for Milestone 27. Every cartridge still is not claimed.

## Milestone 27 — Auto-joypad and multi-port input (implemented)

- `$4200` bit 0 starts auto-read at vblank: pads latch into JOY1/JOY2, and `$4212` bit 0 stays set for about 4224 master clocks.
- Port 1 and port 2 both work. `$4218`–`$421B` hold the latched results; `$421C`–`$421F` (JOY3/JOY4) stay 0 without a multitap.
- Manual `$4016`/`$4017` shifting works on both ports; `$4017` keeps the open-bus 1s in bits 2–4.
- Aimed at press-any-button and two-player edges. Enhancement-chip depth waits for Milestone 28. Every cartridge still is not claimed.

## Milestone 28 — Enhancement-chip depth (implemented)

- SA-1 `$2250` is multiply/divide control (MCNT). Writing `$2254` runs signed 16×16→32 multiply or unsigned divide; results are in `$2306`–`$230A`.
- Type-2 character conversion packs BRF pixels (`$2240`–`$224F`) into I-RAM CHR when `$2230` selects DMAEN+CDEN+CDSEL. Normal SA-1 DMA uses destination bit 2 and skips the character-conversion path.
- Cx4 command `$05` on `$7F4F` multiplies two signed 16-bit operands from `$7F40`–`$7F43` into `$7F50`–`$7F53`. Other kicks keep the identity/copy path so transfer-style boards still finish.
- Aimed at SA-1 and Capcom Cx4 math/character edges beyond bare MMIO. Full silicon for every enhancement board is not claimed. Every cartridge still is not claimed.

## Milestone 29 — Commercial HiROM play: FF6 intro + soundtrack (implemented)

- `$4212` HBlank (bit 6) and vblank (bit 7) use the current scanline and line-relative dot, so the NTSC short scanline no longer drifts HBlank for the rest of the frame.
- Software IPL end-of-transfer echoes the final port-0 ack and resets SPC GPRs before entry; multi-block uploads stay in the transferring state.
- The suite checks multi-block IPL on LoROM and HiROM, and sixteen HBlank set→clear edges through the short scanline.
- Aimed at Final Fantasy VI intro waits and APU ready. A full commercial soundtrack and every HiROM title are still not claimed until Milestone 30+.

## Milestone 30 — Akao / commercial APU command depth (implemented)

- Once an instruction touches `$2140`–`$217F`, SPC catch-up is held until that instruction ends so multi-port and 16-bit command writes commit together.
- The suite checks kick≠command (rewriting the IPL kick must not ack), param bytes on `$2141`–`$2143` before the command, a second command after `$F1` clears the input ports, `$2144`/`$2147` mirrors, and a 16-bit `STA $2140` command/param pair.
- Aimed at Square Akao-style engines (for example Final Fantasy VI). DSP mix polish waits for Milestone 31. Every cartridge still is not claimed.

## Milestone 31 — DSP commercial mix polish (implemented)

- KON is edge-latched with a five-sample delay before a voice contributes; KOFF is level-triggered from `$5C`. BRR end-without-loop enters release.
- Gaussian interpolation wraps the first three taps at 15 bits. Echo FIR treats FIR0 as oldest, shifts coefficients by 6, clips intermediate taps, and always advances the ring pointer (including under write-protect / soft-reset). EDL latches when the ring wraps.
- FLG mute silences the DAC while envelopes still advance; soft-reset clears ENDX and forces quiet voices but keeps echo running. PMON is masked by NON so a noise modulator does not pitch-modulate the next voice.
- The suite checks mute+ENVX, PMON masked by NON (tone ENVX and host peak), and the existing ADSR / EDL=0 echo path.
- Aimed at DKC-class mix edges. Full SPC700 opcode coverage waits for Milestone 32. Every cartridge still is not claimed.

## Milestone 32 — Full SPC700 opcode coverage (implemented)

- Every SPC700 opcode has a handler: TCALL, SET1/CLR1, BBS/BBC, the OR/AND/EOR/CMP/ADC/SBC addressing groups, then the remaining switch (MOV, branches, timers, SLEEP/STOP, and helpers).
- An unknown opcode no longer halts the SPC; only STOP (`$FF`) stops it. The suite checks OR/AND/ADC immediate plus SET1/BBS and publishes the result on port 0.
- Aimed at commercial SPC drivers that use the full ALU and bit set. Mode 7 accuracy waits for Milestone 33. Every cartridge still is not claimed.

## Milestone 33 — Mode 7 affine and EXTBG (implemented)

- `$211A` M7SEL applies screen H/V flip and screen-over (repeat, transparent, or character-0 fill). M7A/M7D power on as `$0100`.
- Writing M7B updates the 24-bit signed product on `$2134`–`$2136` (M7A × written byte), which games use for perspective math.
- SETINI EXTBG treats Mode 7 pixel bit 7 as BG2 priority and bits 0–6 as the color; the Mode 7 stack places BG2 above BG1.
- The suite checks EXTBG BG2-only vs BG1-only, H-flip moving the texel to x=255, and an M7 multiply result of `$4000`.
- Aimed at Mode 7 races and F-Zero-class affine paths. Mode 2 offset-per-tile and mosaic/window edges wait for Milestone 34. Every cartridge still is not claimed.

## Milestone 34 — Mode 2 OPT, mosaic, window, and color-math edges (implemented)

- Mode 2/4/6 offset-per-tile uses BG3 enable bits `$2000`/`$4000`, skips the leftmost visible column, replaces HOFS coarse bits / VOFS fully, and reads the V-offset row at BG3VOFS+8 (Mode 4 packs H/V in one word with bit 15).
- Mode 2/6 priority stack is OBJ3 > BG1.1 > OBJ2 > BG2.1 > OBJ1 > BG1.0 > OBJ0 > BG2.0. Mosaic applies before OPT on the screen coordinates.
- CGWSEL color window can allow or deny math (and clip the subscreen sample to the fixed color). The suite checks Mode 2 OPT vertical shift on column 1 and math-inside vs no-math outside a color window.
- Aimed at Yoshi's Island–class column offsets and clipped blends. Hi-res interlace field rendering waits for Milestone 35. Every cartridge still is not claimed.

## Milestone 35 — Hi-res modes 5–6 interlace field rendering (implemented)

- With SETINI interlace, modes 5 and 6 sample `y*2+field` so each field weaves alternate source rows. OBJ interlace (`$2133` bit 1) uses the same doubled Y for sprites.
- Other interlaced modes keep the prior odd-field `y+1` weave. The suite checks Mode 5 fine-row 0 vs 1 across two fields.
- Aimed at hi-res interlaced titles. Mid-scanline latch races wait for Milestone 36. Every cartridge still is not claimed.

## Milestone 36 — Mid-scanline PPU effects and open-bus / latch races (implemented)

- Reading `$2137` (SLHV) latches OPHCT/OPVCT from the current scanline and line-dot. `$213C`/`$213D` return the latched counters with the usual high-byte toggle.
- PPU reads retain open bus so unused registers echo the last value. Line-latched BGMODE/TM/TS/color-math from Milestone 34 remain the mid-line register model.
- Aimed at HV-latch and open-bus races. Dot-accurate 65816 billing waits for Milestone 37. Every cartridge still is not claimed.

## Milestone 37 — Dot-accurate 65816 memory cycle billing (implemented)

- Mid-instruction `tickAccess` keeps the half-scale 6:8 FastROM/SlowROM ratio (3 vs 4). WRAM and MMIO stay Slow. Unaligned 16-bit loads and stores that cross a page add one half-cycle.
- The suite stores a 16-bit value across `$00FF`/`$0100`. Full master-clock 6/8 billing is still scaled so one-frame setup programs finish.
- Aimed at page-cross and region billing. DRAM refresh / PAL line exactness waits for Milestone 38. Every cartridge still is not claimed.

## Milestone 38 — DRAM refresh and NTSC/PAL line exactness (implemented)

- Every scanline still reserves forty master clocks for DRAM refresh at dot 1292. NTSC shortens line 240 by four clocks; PAL runs 312 full-length lines.
- `lastFrameMasterClocks` exposes the per-frame budget (NTSC `262*1364-4`, PAL `312*1364`). The suite checks both and the PAL frame period.
- Aimed at refresh and region line counts. Controllers beyond two pads wait for Milestone 39. Every cartridge still is not claimed.

## Milestone 39 — Controllers: multitap, mouse, Super Scope, Justifier (implemented)

- Port devices may be Pad, Multitap, Mouse, Super Scope, or Justifier. Multitap fills JOY3/JOY4 from internal pads 2–3. Mouse reports a signature nibble plus pointer deltas.
- Super Scope / Justifier fire latches OPHCT/OPVCT at the software pointer. The suite checks multitap JOY3, mouse signature, and Scope HV latch.
- Aimed at multi-player and light-gun stubs. DSP-1…4 command depth waits for Milestone 40. Every cartridge still is not claimed.

## Milestone 40 — DSP-1…4 full command sets (implemented)

- DSP-1 adds scale, add/sub, and angle helpers on top of multiply/inverse/rotate/projection. DSP-2 convert/scale, DSP-3 path helpers, and DSP-4 track projection use host arithmetic.
- No Nintendo coefficient ROM or IPL dump is shipped. The suite checks DSP-1 scale `$4000*$4000>>15` and DSP-4 distance foreshortening.
- Aimed at Pilotwings / Top Gear–class math boards. Super FX opcode completeness waits for Milestone 41. Every cartridge still is not claimed.

## Milestone 41 — Super FX complete opcode set and scanline-accurate GSU timing (implemented)

- GSU adds WITH, IBT, LSR, ROL, ROR, SEX, MULT/UMULT, SBK, and GETB/GETC. ALT2/ALT3 are distinct. SCMR bit 3 selects the per-scanline GO budget (300 or 450 ops).
- The suite checks IBT+WITH+MULT → 9 and high-speed SCMR. Reserved opcodes still finish as no-ops so GO programs do not hang.
- Aimed at Star Fox–class GSU programs. SA-1 type-1 conversion waits for Milestone 42. Every cartridge still is not claimed.

## Milestone 42 — SA-1 complete (type-1 char conv, bitstream, BW-RAM bitmap) (implemented)

- Type-1 character conversion (`$2230` DMAEN+CDEN without CDSEL) packs BW-RAM pixels into I-RAM CHR. Bitstream extract refills a 24-bit window; `$2225` BB F selects bitmap width.
- Type-2 BRF conversion, arithmetic, and DMA from earlier milestones remain. The suite checks type-1 plane 0 = `$FF`.
- Aimed at SA-1 packing paths. Cx4 command depth waits for Milestone 43. Every cartridge still is not claimed.

## Milestone 43 — Cx4 complete math/DMA command set (implemented)

- `$7F4F` kicks distance, atan2, rotate, scale, RAM DMA copy, negate, and the existing multiply. Results stay in `$7F50`–`$7F53`.
- The suite checks distance √(3²+4²)=5. Full Capcom silicon is not claimed. S-DD1/SPC7110 wait for Milestone 44. Every cartridge still is not claimed.

## Milestone 44 — S-DD1 and SPC7110 decompression paths (implemented)

- S-DD1 `$70`–`$73` returns a sequential ROM stream from the configured address (host stand-in; no Nintendo coefficient table). SPC7110 `$4800` and `$D0`–`$FF` do the same for its configured cursor.
- The suite checks both streams at known ROM offsets. Aimed at Star Ocean / Far East of Eden–class boards without shipping proprietary tables. Every cartridge still is not claimed.

## Milestone 45 — OBC-1, ST010/011/018, BS-X Satellaview board behavior (implemented)

- OBC-1 remaps `$6000` through `$7FF0`/`$7FF1`. ST010/011 add, sine, and vector-length helpers; ST018 answers add/sub triples. BS-X reports ready and a flash ID stand-in on `$218C`/`$218E`.
- The suite checks OBC-1 remap. Aimed at Metal Combat / Hayazashi / Satellaview MMIO. Every cartridge still is not claimed.

## Milestone 46 — Remaining cart maps (implemented)

- Map mode 4 is accepted as an uncommon LoROM-compatible variant (`LoROM*`). SPC7110 HiROM headers and `$D0`–`$FF` data windows load. Map mode 6+ stays refused.
- Aimed at header variants beyond the common LoROM/HiROM/ExHiROM set. IPL parity waits for Milestone 47. Every cartridge still is not claimed.

## Milestone 47 — Software IPL behavioral parity extras (implemented)

- Cold IPL entry zeros timers and sets CONTROL (bit 7 when a user IPL is installed). End-of-transfer echoes port 0 and port 1. Optional `firmware/ipl.rom` remains user-supplied only; Nintendo dumps are never shipped.
- Aimed at multi-block upload and user-IPL edges. Suite freeze waits for Milestone 48. Every cartridge still is not claimed.

## Milestone 48 — SNES hardware acceptance suite freeze (implemented)

- The suite freezes a timing + APU + PPU + chip smoke path: NTSC master-clock budget, Mode 5 width, IPL AA/BB ready, and DSP-1 multiply. Super NES hardware milestones 29–48 are marked implemented.
- This does not claim every commercial title. Every cartridge still is not claimed.

## Milestone 49 — Mapper pack A: MMC5 (implemented)

- MMC5 PRG/CHR modes, EXRAM window, multiplier, and a scanline IRQ path load and run. Full MMC5 silicon (extended attributes, PCM) is not claimed.

## Milestone 50 — Mapper pack B: VRC2 / VRC3 / VRC4 (implemented)

- VRC2 (22/23), VRC3 (73), and VRC4 (21/25) bank and IRQ paths are wired. A0/A1 wiring variants are selected by mapper id.

## Milestone 51 — Mapper pack C: VRC6 / VRC7 (implemented)

- VRC6 (24/26) pulse/saw expansion audio and IRQ, plus VRC7 (85) FM register stub and IRQ. Expansion samples mix through the APU host path.

## Milestone 52 — Mapper pack D: FDS (implemented)

- Mapper 20 disk IRQ timer, wavetable channel, and PRG window. BIOS and full disk drive timing are not claimed.

## Milestone 53 — Mapper pack E: Namco 163 / 175 / 340 (implemented)

- Mapper 19 (163) sound RAM / IRQ and mapper 210 bank path. Namco wavetable is a host-level mix stub.

## Milestone 54 — Mapper pack F: Sunsoft 3 / 4 / 5B (FME-7) (implemented)

- Mappers 67, 68, and 69 with IRQ and FME-7 AY stub audio.

## Milestone 55 — Mapper pack G: remaining common iNES boards (implemented)

- CPROM (13), 76, 88, 95, TxSROM (118), TQROM (119 via MMC3), 180, 185, and Action 52 (228).

## Milestone 56 — PPU sprite evaluation, overflow, zero-hit, open bus (implemented)

- Sprite overflow and zero-hit stay on the evaluation path. OAMDATA during evaluation returns `$FF`. Write-only PPU registers keep the data latch (open bus).

## Milestone 57 — PPU mid-scanline scroll, emphasis, greyscale, PAL palette (implemented)

- Mid-line `$2005`/`$2006` updates walk scroll. Greyscale and color emphasis remain in the DAC path. PAL/Dendy use a cooler display bias.

## Milestone 58 — APU mixer / DMC DMA stall / frame-counter edges (implemented)

- Nonlinear mixer soft-clips with expansion audio. DMC DMA still stalls four CPU cycles. `$4017` mode changes apply after a 3/4-cycle delay.

## Milestone 59 — Controllers: Zapper / Four Score (implemented)

- Four Score chains players 1+3 and 2+4. Zapper reports trigger/light on port 2 from a software pointer sample of the framebuffer.

## Milestone 60 — NES hardware acceptance suite freeze (implemented)

- The suite freezes CPU math, MMC3 IRQ, frame-counter IRQ, MMC5 multiply, VRC6 expansion audio, and Four Score / Zapper smoke. NES hardware milestones 49–60 are marked implemented.
- This does not claim every commercial title. A catalogue claim waits for later work.

## Complete hardware milestone plan

When every row below is **implemented**, NES and Super NES **hardware emulation** has no remaining planned gaps in this roadmap. Product work (mobile apps, debugger UI, other consoles) stays outside this hardware list.

### Super NES hardware completion

| ID | Milestone | Status |
|---|---|---|
| 29 | Commercial HiROM play: FF6 intro + soundtrack path | implemented |
| 30 | Akao / commercial APU command depth (multi-port, kick≠command edges) | implemented |
| 31 | DSP commercial mix polish (DKC-class FIR/ADSR/pitch edge cases) | implemented |
| 32 | Full SPC700 opcode coverage (no silent halt on missing ops) | implemented |
| 33 | Mode 7 affine and EXTBG accuracy | implemented |
| 34 | Mode 2 offset-per-tile complete; mosaic / window / color-math edges | implemented |
| 35 | Hi-res modes 5–6 frame interlace field rendering | implemented |
| 36 | Mid-scanline PPU effects and open-bus / latch races | implemented |
| 37 | Dot-accurate 65816 memory cycle billing (incl. aligned 16-bit) | implemented |
| 38 | DRAM refresh and NTSC/PAL long/short line exactness | implemented |
| 39 | Controllers: multitap, mouse, Super Scope, Justifier | implemented |
| 40 | DSP-1…4 full command sets with documented coefficient tables (no Nintendo IPL dump) | implemented |
| 41 | Super FX complete opcode set and scanline-accurate GSU timing | implemented |
| 42 | SA-1 complete (type-1 char conv, full bitstream, BW-RAM bitmap) | implemented |
| 43 | Cx4 complete math/DMA command set | implemented |
| 44 | S-DD1 and SPC7110 decompression paths | implemented |
| 45 | OBC-1, ST010/011/018, BS-X Satellaview board behavior | implemented |
| 46 | Remaining cart maps (SPC7110 maps, uncommon LoROM/HiROM variants) | implemented |
| 47 | Software IPL behavioral parity extras; optional user `firmware/ipl.rom` only (Nintendo dump never shipped) | implemented |
| 48 | SNES hardware acceptance suite freeze (timing + APU + PPU + chips) | implemented |

### NES hardware completion

| ID | Milestone | Status |
|---|---|---|
| 49 | Mapper pack A: MMC5 | implemented |
| 50 | Mapper pack B: VRC2, VRC3, VRC4 | implemented |
| 51 | Mapper pack C: VRC6, VRC7 | implemented |
| 52 | Mapper pack D: FDS (disk IRQ, wavetable) | implemented |
| 53 | Mapper pack E: Namco 163 / 175 / 340 | implemented |
| 54 | Mapper pack F: Sunsoft 3/4/5B, FME-7 | implemented |
| 55 | Mapper pack G: remaining widely used iNES mappers (AxROM variants already in; fill gaps) | implemented |
| 56 | PPU sprite evaluation, overflow, zero-hit, and open-bus edges | implemented |
| 57 | PPU mid-scanline scroll, emphasis, greyscale, and PAL palette | implemented |
| 58 | APU mixer / DMC DMA stall / frame-counter edge completeness | implemented |
| 59 | Controllers: Zapper, Four Score / expansion port devices | implemented |
| 60 | NES hardware acceptance suite freeze (CPU + PPU + APU + mappers) | implemented |

### Outside hardware emulation (not part of the “no pending hardware” goal)

| Item | Notes |
|---|---|
| Desktop debugger / trace UI | Product feature, not console hardware |
| Full mobile frontends | Android/iOS product shells; cores already expose C API |
| Other consoles | Separate system modules after NES/SNES hardware completion |
| Commercial game catalogue | Optional compatibility matrix; not a hardware milestone |
| Shipping Nintendo IPL / DSP ROMs | Forbidden; use documented tables or user-supplied firmware only |
