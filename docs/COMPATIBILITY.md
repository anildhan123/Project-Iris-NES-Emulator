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

Background pattern bytes are fetched while the dot that needs them is drawn. Sprite pattern bytes are fetched on dots 257–320 for the next line. Mappers other than NROM, MMC1, UxROM, CNROM, MMC3, AxROM, MMC2, MMC4, Color Dreams, BNROM, GxROM, Camerica, VRC1, NINA-003, mapper 87, DxROM (206), and BF9096 (232) are rejected.

## Picture regression

After 90 idle frames, Iris Demo's 256×240 picture hashes to `ef26962c33f1a46db91ef4b16eb74ddbb6f66e807ea9cb82d6dd82219b744ba1`. A second run must produce the same hash. Holding Right must produce a different hash. The audio of that run must also match a second run. This locks the original demo. It does not claim byte-accurate output for other games.

The debug suite fails if those 180 demo frames average 100 ms or more. That limit catches a hang. The Release measurement below is the frame time for this candidate.

## Super NES

The suite loads LoROM and HiROM, draws the Iris Demo marker, and checks the picture features from milestones 16 and 17. A 65816 program uploads an SPC700 payload through `$2140`–`$2143`, and the output port returns `$5A`. A second upload plays a BRR sample through the DSP. That host buffer is 48000 Hz stereo with Gaussian interpolation, and both channels peak above silence. An idle frame still produces samples. Those samples stay silent when the cartridge did not upload a program. The upload protocol is built in. A user may place a 64-byte IPL image at `firmware/ipl.rom` in the data directory; Nintendo's dump is not shipped. A 16-bit store to `$2140` can finish that transfer: the boot handshake reads both port bytes after the instruction, and an 8-bit index sign-extends when it moves into the 16-bit accumulator. A save state restores work RAM for the same game, and a damaged file or a state from another game is refused. A cartridge that uploads an SPC700 soundtrack plays through that stereo DSP mix.

A DSP-1 image multiplies `$4000` by `$4000` and reads `$2000`, then checks inverse of zero and a zero-angle triangle. Additional DSP-1 geometry commands, including projection and attitude helpers, answer with host arithmetic. DSP-2, DSP-3, and DSP-4 boards load and finish their command transfers. A Super FX image stores `$A5` through the GSU and plots that color, including packed 2-bit and 4-bit screens when SCMR selects them, and runs multiply, XOR, subtract, and link helpers. An SA-1 image starts the coprocessor at its reset vector and reads `$5A` back from I-RAM, and SA-1 DMA can copy between I-RAM and BW-RAM. An ExHiROM image reads `$A5` from the first half at `$C0:0000` and `$5A` from the second half at `$40:0000`. A map-mode 5 byte in the LoROM header slot is still refused. A HiROM program that writes work RAM at `$7E:0010` reads zero from `$FE:0010`. Not every enhancement-chip title is claimed to play end to end.

A program can turn NMI off and still see the vblank flag in `$4210`. `$4212` bit 6 is set in the last portion of each scanline and clear again on the next line, which is the wait Final Fantasy VI uses before it continues. Modes 5 and 6 draw 512 dots per line. Each scanline reserves forty clocks for DRAM refresh, and NTSC shortens one scanline by four clocks. HDMA on a scanline leaves fewer instructions before vblank than the same program with HDMA off. A 65536-byte DMA does not reach the store that follows it in that frame, and a one-byte DMA does. A vertical IRQ and a horizontal IRQ each run their handler. This does not claim dot-accurate timing inside an instruction, and it does not claim that every cartridge plays.

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
