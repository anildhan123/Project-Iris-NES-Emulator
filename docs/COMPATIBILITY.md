# Compatibility

Version 0.22.0 records what this Windows build was tested to do. The Ubuntu and macOS jobs cited below compiled and tested 0.9.0. It is not a catalogue of commercial games, and it does not include those games.

Run the suite from a configured build tree:

```powershell
ctest --preset windows-msvc-debug --output-on-failure
```

`iris_unit` is the regression suite. `iris_profile` runs `iris --profile` on Iris Demo. The same profile command is part of the Release job in Windows CI.

## CPU

The CPU tests cover official opcodes, the unofficial opcodes, ADC with the decimal flag ignored, a taken branch that crosses a page, JSR and RTS, the indirect JMP page-wrap bug, NMI before the next instruction, and IRQ waiting until the instruction after CLI.

## Cartridge and mappers

The suite accepts iNES and NES 2.0 NROM, reports mirroring, infers CHR RAM when a ROM has no CHR, and rejects a malformed file, a missing file, and a non-NES console byte. An unsupported mapper is refused and the game already running stays loaded.

Original programs, not a third-party test ROM, check:

- UxROM switches the low PRG bank and leaves the last bank fixed.
- CNROM selects an 8 KiB CHR bank, and submapper 0 has a bus conflict.
- MMC1 loads a PRG bank from five register writes, and a second write in the same instruction is ignored.
- MMC3 raises an IRQ after a filtered A12 rise.
- The frame counter raises an IRQ in four-step mode.
- DMC playback fetches a sample and moves the mixer.
- A NES 2.0 header selects PAL or Dendy frame length. NTSC is the default.

Background pattern bytes are fetched while the dot that needs them is drawn. Sprite pattern bytes are fetched on dots 257–320 for the next line. Mid-scanline scroll writes update the walk counters. Greyscale and emphasis stay in the DAC path; PAL/Dendy use a cooler display bias. OAMDATA during sprite evaluation returns `$FF`. `$4017` frame-counter mode changes apply after a short delay; DMC DMA stalls four CPU cycles; mapper expansion audio mixes into the nonlinear APU output. Four Score and Zapper devices are available on the controller ports.

The suite also checks MMC5 multiply, VRC6 expansion audio, Four Score / Zapper bit reports, and a Milestone 60 freeze covering CPU math, MMC3 IRQ, and frame-counter IRQ. Unsupported mapper ids are still refused without unloading a running game.

## Picture regression

After 90 idle frames, Iris Demo's 256×240 picture hashes to `ef26962c33f1a46db91ef4b16eb74ddbb6f66e807ea9cb82d6dd82219b744ba1`. A second run must produce the same hash. Holding Right must produce a different hash. The audio of that run must also match a second run. This locks the original demo. It does not claim byte-accurate output for other games.

The debug suite fails if those 180 demo frames average 100 ms or more. That limit catches a hang. The Release measurement below is the frame time for this candidate.

## Super NES

The suite loads LoROM and HiROM, draws the Iris Demo marker, and checks the picture features from milestones 16 and 17. A 65816 program uploads an SPC700 payload through `$2140`–`$2143`, and the output port returns `$5A`. OR/AND/ADC immediate and SET1/BBS run without halting the SPC (only STOP stops it). A second upload plays a BRR sample through the DSP. That host buffer is 48000 Hz stereo with Gaussian interpolation, and both channels peak above silence. An idle frame still produces samples. Those samples stay silent when the cartridge did not upload a program. The upload protocol is built in and clears APU RAM when the software IPL path is entered. An SPC700 timer overflow with interrupts enabled wakes `SLEEP` through `$FFDE`, which commercial engines use to tick music. After boot, a program can send a command on `$2140` and read an ack on the ports (including the `$2144` mirror); 16-bit stores to `$2140` commit both bytes before the SPC runs. A user may place a 64-byte IPL image at `firmware/ipl.rom` in the data directory; Nintendo's dump is not shipped. A 16-bit store to `$2140` can finish that transfer: the boot handshake reads both port bytes after the instruction, and an 8-bit index sign-extends when it moves into the 16-bit accumulator. A save state restores work RAM for the same game, and a damaged file or a state from another game is refused. A cartridge that uploads an SPC700 soundtrack plays through that stereo DSP mix with ADSR rate tables, Gaussian taps that step on sample edges (15-bit wrap on the first three taps), noise clocked by FLG, and echo stored in APU RAM at ESA (including the EDL=0 one-sample buffer, FIR0=oldest, >>6 with clip15, ring advance under write-protect). KON uses a five-sample delay; KOFF is level-triggered; BRR end-without-loop enters release. FLG mute keeps the host quiet while ENVX still advances; soft-reset quiets voices but keeps echo. PMON is masked when the modulator voice uses noise. A fast ADSR attack reaches a high ENVX. Not every commercial soundtrack is proven end to end.

A DSP-1 image multiplies `$4000` by `$4000` and reads `$2000`, then checks inverse of zero and a zero-angle triangle. Additional DSP-1 geometry commands, including projection, attitude helpers, and scale/add/angle replies, answer with host arithmetic (no coefficient ROM). DSP-2, DSP-3, and DSP-4 boards load and finish their command transfers, including DSP-4 track projection. Cx4, S-DD1, SPC7110, OBC-1, ST010/011/018, and BS-X boards load and answer their MMIO slices; those paths are not full silicon. A Super FX image stores `$A5` through the GSU and plots that color through an 8-pixel cache, including packed 2-bit and 4-bit screens when SCMR selects them, on a per-scanline GO budget, and runs multiply, XOR, subtract, and link helpers. An SA-1 image starts the coprocessor at its reset vector and reads `$5A` back from I-RAM; SA-1 DMA can copy between I-RAM and BW-RAM, the bitstream ports accept a packed extract, multiply/divide through `$2250`–`$2254` returns product or quotient/remainder on `$2306`–`$230A`, and type-2 character conversion packs BRF rows into I-RAM. SA-1 type-1 conversion and complete bitstream hardware remain incomplete. A Cx4 board multiplies through command `$05` on `$7F4F` in addition to its MMIO slice. An ExHiROM image reads `$A5` from the first half at `$C0:0000` and `$5A` from the second half at `$40:0000`. A map-mode 5 byte in the LoROM header slot is still refused, as are exotic map modes beyond LoROM, HiROM, ExHiROM, FastROM, LoROM variants 2 / `$A`, and SA-1. A HiROM program that writes work RAM at `$7E:0010` reads zero from `$FE:0010`. The suite proves these features; it does not claim that every cartridge plays.

A program can turn NMI off and still see the vblank flag in `$4210`. `$4212` bit 6 is set in the last portion of each scanline and clear again on the next line, which is the wait Final Fantasy VI uses before it continues. `$4212` bit 7 (and NMI) use line 225, or 240 when SETINI selects overscan; the suite checks that overscan delays that flag. When `$4200` bit 0 is set, auto-joypad read latches ports 1 and 2 into `$4218`–`$421B` at vblank and holds `$4212` bit 0 for about three scanlines; a multitap fills `$421C`–`$421F` from pads 2–3. Manual `$4016`/`$4017` shifting works on both ports. Mouse, Super Scope, and Justifier stubs report signatures or latch HV at a software pointer. Modes 5 and 6 draw 512 dots per line; with SETINI interlace they sample alternate source rows per field. Mode 2/4/6 offset-per-tile follows BG3 enable bits and the first-column skip; mosaic runs before OPT. Mode 7 honors `$211A` flip and screen-over, SETINI EXTBG (pixel bit 7 → BG2 priority), and the M7A×M7B product on `$2134`–`$2136`. CGWSEL can gate color math with the color window. `$2133` can select overscan (239 lines) and interlace field reporting in `$213F`. `$2137` latches OPHCT/OPVCT; PPU reads keep open bus. BGMODE, TM, TS, and color-math selects latch at the start of each visible line. HDMA runs at HBlank on each line (including vblank while channels last). Each scanline reserves forty clocks for DRAM refresh, NTSC shortens one scanline by four clocks, and PAL uses 312 lines. Memory accesses inside an instruction charge FastROM or SlowROM clocks (half-scale 6:8), with an extra half-cycle on unaligned 16-bit page crosses. HDMA on a scanline leaves fewer instructions before vblank than the same program with HDMA off. A 65536-byte DMA does not reach the store that follows it in that frame, and a one-byte DMA does. A vertical IRQ and a horizontal IRQ each run their handler. This does not claim full silicon for every mid-scanline race or enhancement chip, and it does not claim that every cartridge plays.

Commercial titles such as Final Fantasy VI are not marked compatible yet. Milestones 29–48 freeze the Super NES hardware acceptance path. Milestones 49–60 freeze the NES hardware acceptance path (mapper packs, PPU/APU edges, Zapper / Four Score). A commercial catalogue claim waits for later work. Keyboard **Enter** is Start; **Z** is A (and Y on Super NES), not Start. The complete NES and Super NES hardware milestone list is in `docs/ROADMAP.md` under Complete hardware milestone plan.

## Frame time

`iris --profile rom.nes` loads that ROM, runs 300 frames without opening a window, and prints the region's target frame time, the average host time, and the slowest frame. Samples are not played. The command does not change the PPU output.

Release measurement of Iris Demo on the development machine, Windows 11, Release build:

```text
Iris Emulator 0.7.0
Profile: iris_demo (NROM, NTSC)
Frames: 300
Target: 16.639 ms
Average: 2.455 ms
Slowest: 6.447 ms
```

The average is under the NTSC frame period on that machine. Another PC will print different host times. The target line is the emulated frame period. This is a measurement, not a compatibility score.

## Crash and error handling

A ROM this build cannot play still produces the existing error dialog, and a game that was already running stays loaded. A damaged save state is refused. A malformed configuration file is left on disk and the session uses defaults.

An unexpected C++ exception, or a Windows access violation, integer division by zero, or stack overflow, is appended to `logs/crash.log` and shown in a dialog. Configuration is not written after that failure. If a debugger is attached, the Windows exception is left for the debugger.

## Release candidate

0.22.0 is an unsigned build. The Windows executable's version resource says 0.22.0. Build it with `cmake --build --preset windows-msvc-release`. Package it with `cpack -G ZIP -C Release` from the build directory. The zip contains the executable, Iris Demo, the readme, and the license files. It is not a signed installer.

The frame-time numbers above were measured for 0.7.0 on this Windows 11 machine. A later `--profile` run prints `Iris Emulator 0.22.0` and its own host times. The target line stays the NTSC frame period. The 0.7.0 numbers were not remeasured for 0.10.0.

## Portability

The development machine is Windows. The C API test and the Windows and XDG data-root tests ran here. `iris --version` on this machine prints `Iris Emulator 0.22.0`. A second configure with the desktop turned off built the core and passed the unit tests, and it did not produce the window executable.

The desktop window draws its own title bar, caption buttons, and border. Themes are Blue, Olive, and Silver. Fullscreen hides that frame. This is the window chrome; it does not change the emulated picture.

Linux and macOS desktop builds are the `linux-debug` and `macos-debug` presets. SDL 3.4.18 is compiled from source. The Ubuntu job installs `libxtst-dev` because SDL's X11 backend requires XTEST. OpenGL function pointers use `APIENTRY` from SDL's OpenGL header, which is `__stdcall` on Win32 and empty for GCC and Apple Clang. Linux needs an OpenGL 3.3 Core driver. macOS requests OpenGL 3.2 compatibility; the picture shaders are still GLSL 330, and a shader failure shows crisp pixels. The macOS binary is not signed or notarized.

On commit `0a1547c`, GitHub Actions run [38257147101](https://github.com/anildhan123/Project-Iris-NES-Emulator/actions/runs/38257147101) passed both Windows jobs: MSVC debug (configure, build, test, version) and MSVC release (configure, build, version, profile). Portability run [38257147297](https://github.com/anildhan123/Project-Iris-NES-Emulator/actions/runs/38257147297) passed Ubuntu debug and macOS debug. Each of those jobs configured, built, ran `ctest`, and printed the version. A passing compile and `ctest` is not a launched window. This machine did not open an Ubuntu or macOS window, and those jobs do not start one.

`ports/android` is a JNI library compiled only by the Android NDK. `ports/ios/iris_bridge.h` is included by the Windows unit test so the C declarations compile. Neither port was run as an app, and this machine did not run the NDK or Xcode.
