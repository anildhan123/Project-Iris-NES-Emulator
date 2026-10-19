# Architecture

Iris is an emulator platform. The NES core is the first system module. This document describes the boundaries that keep additional systems possible.

## Layers

```text
iris_app
  -> iris_frontend
       -> iris_emulator
       -> iris_platform -> iris_common
       -> iris_common
  -> iris_nes
       -> iris_emulator
       -> iris_common
  -> iris_snes
       -> iris_emulator
       -> iris_common
iris_core
  -> iris_nes
iris_jni (Android only)
  -> iris_core
```

| Module | Responsibility | Must not depend on |
|---|---|---|
| `iris_common` | Version, errors, logging, configuration values and JSON validation | SDL, ImGui, Win32, a system core |
| `iris_platform` | Data directories, safe path joins, atomic file replacement | SDL, ImGui, UI widgets |
| `iris_emulator` | `IEmulatedSystem`, frame and audio views, controller button bits | SDL, ImGui, CPU, PPU, or mapper types |
| `iris_nes` | iNES parsing, NROM, MMC1, UxROM, CNROM, MMC3, AxROM, MMC2, MMC4, Color Dreams, BNROM, GxROM, Camerica, VRC1, NINA-003, mapper 87, DxROM, BF9096, CPU, bus, PPU, APU, controllers | SDL, ImGui, Win32 UI |
| `iris_snes` | LoROM, HiROM, and ExHiROM, 65816, SRAM, DMA and HDMA on the scanline, horizontal and vertical IRQ, horizontal blank in `$4212`, DRAM refresh and the NTSC short scanline, windows, fixed and subscreen color math, modes 0–7 including 512-dot modes 5 and 6, four backgrounds, scroll, priority, offset-per-tile, mosaic, sprite sizes, SPC700, stereo DSP with Gaussian BRR, DSP-1 through DSP-4, Super FX, SA-1, save states, and optional user IPL at `firmware/ipl.rom` (Nintendo firmware is not shipped) | SDL, ImGui, Win32 UI, the NES core |
| `iris_core` | C functions around the NES system: load, run, frame, audio, and save state | SDL, ImGui, CPU, PPU, or mapper types in the header |
| `iris_frontend` | SDL3 window, OpenGL context, ImGui presentation, file dialog, audio queue | CPU, PPU, APU, or mapper types |
| `iris_app` | Process startup, `--version`, optional ROM path, shutdown | Emulator internals |

`IRIS_DESKTOP` defaults to on. Android and iOS configure it off, so those builds do not fetch SDL or Dear ImGui and do not build the window. `iris_core` stays in every build. Its header, `src/core/iris.h`, is the stable boundary: an opaque `IrisSystem`, integer results, and caller-owned error buffers. The picture and sample pointers stay valid until the next load, unload, run, or destroy on that handle. `ports/android/iris_jni.cpp` is the JNI bridge and is compiled only when `ANDROID` is set. `ports/ios/iris_bridge.h` is the same C API for a future Swift or Objective-C caller. Neither port has a screen, a touch control, or an audio-focus policy.

The desktop calls `IEmulatedSystem`. It does not read or write CPU, PPU, or mapper state. `describeContent` reads a ROM's title, detail, and content id without replacing the running game. That content id is the same SHA-256 stored for a loaded cartridge. `iris_frontend` links the interface target only. `iris_app` is what links `iris_nes` and `iris_snes`. With no ROM on the command line, the window shows a system screen for NES or Super NES before the worker starts. A `.sfc` or `.smc` path selects Super NES. Any other path stays on NES. After a system is open, File → System switches between the two. Opening a `.nes` file while Super NES is current, or a `.sfc` or `.smc` file while NES is current, switches before the file is loaded.

## Milestone 2 runtime

1. Resolve the data directory, start logging, and load configuration, as in Milestone 1.
2. Create the NES system and open the window.
3. If a ROM path was passed on the command line, or the user chooses File → Open Game, parse it as iNES or NES 2.0.
4. NROM, MMC1, UxROM, CNROM, MMC3, AxROM, MMC2, MMC4, Color Dreams, BNROM, GxROM, Camerica, VRC1, NINA-003, mapper 87, DxROM, and BF9096 run when the header sizes match that board. Anything else is reported and the previous game, if any, stays loaded.
5. While the game is running, the window calls `runFrame` once per frame period reported by the system, not once per host refresh. NTSC is about 16.64 ms. PAL and Dendy are longer. Pause skips that call.
6. The completed 256×240 RGBA picture is uploaded unchanged. A host shader samples that texture into a separate image. Nearest is the default. Smooth, sharp, and CRT sampling, then brightness, contrast, saturation, and gamma, never write back to the PPU. Save thumbnails use the PPU texture.
7. The picture is integer-scaled to fit, or to a chosen 1×–6× size. If that size does not fit, the next smaller integer is used and the status bar explains it. Pixel aspect keeps each emulated pixel square in the window; 4:3 uses a four-by-three box. Fullscreen is borderless. Samples from that frame are queued to the audio device. A missing audio device does not stop the picture.

The desktop runs emulation on a worker thread and copies the finished picture and samples back to the window. Save, load, reset, and opening a game take the same lock.

## NES behavior in this version

- Official and unofficial 6502 opcodes, including the indirect JMP page-wrap bug. A jammed opcode stops the CPU. ADC and SBC stay binary when the decimal flag is set.
- ADC and SBC stay binary even when the decimal flag is set.
- NROM: 16 KiB PRG is mirrored at `$C000`. CHR may be ROM or RAM. A trainer, when present, is copied to `$7000`.
- UxROM switches a 16 KiB bank at `$8000` and fixes the last bank at `$C000`. Submapper 0 ANDs the written value with the ROM byte at that address. CNROM switches 8 KiB CHR banks the same way.
- MMC1 uses a five-write shift register. A second write before the next CPU cycle is ignored. MMC3 banks PRG and CHR and raises IRQ when PPU A12 rises after staying low.
- The PPU steps three dots per NTSC or Dendy CPU cycle, and 3.2 dots per PAL CPU cycle. Visible dots fetch the background pattern for that tile and draw one pixel. Sprite patterns are fetched on dots 257–320 for the next line. A12 for mapper IRQ is still the filtered rendering signal. NMI is raised when vblank begins with NMI enabled.
- NTSC is 262 scanlines and skips one dot on odd rendered frames. PAL and Dendy are 312 scanlines and do not skip that dot. The host frame period comes from `framePeriodNs()`.
- The APU mixes pulse, triangle, noise, and DMC. The frame counter follows the NTSC or PAL step sequence, including the frame IRQ. Dendy uses the NTSC APU sequence with its own CPU clock. A DMC sample read stalls the CPU for four cycles.
- Controller bits are A, B, Select, Start, Up, Down, Left, Right. Keyboard mapping lives in the shell.
- Battery-backed PRG RAM is loaded from `saves/{content id}.sav` when a game opens and written back when that game closes or the process exits. Reset leaves it in place. It is not part of a save state.

## Save states

The shell asks `IEmulatedSystem` to save, load, and inspect a state. It does not read CPU, PPU, or mapper fields.

A state file starts with the magic `IRS1`, format version 1, the NES system id, and the emulator version. It then stores a SHA-256 of the cartridge identity (mapper, submapper, region, CHR RAM size or CHR ROM bytes, and PRG ROM), the mapper id, the region, a feature-flag byte that must be zero, a timestamp, the title, an optional description, a 64×60 thumbnail, and length-delimited component chunks. A CRC32 covers every preceding byte. Chunks are `CPU `, `PPU `, `APU `, `MAP `, `CART`, `PAD `, `BUS `, and `TIME`. RAM lives inside `BUS`. An unknown chunk, a bad checksum, or a version this build does not know is rejected before the running machine changes. A failed apply restores the components captured just before the attempt.

Files live under the data directory: `states/{content id}/slot-01.irstate` through `slot-10.irstate`. Writes go to a temporary file and replace the destination only after the bytes are on disk. Reads are capped at 4 MiB. The shell keeps ten slots.

## Configuration schema

Schema version 7. A version 1 file, which has no `video` object, a version 2 file, which has `video` but no audio or input, a version 3 file, which has no controller ports or overscan, a version 4 file, which has no region choice, a version 5 file, which has no turbo keys, and a version 6 file, which has no fast-forward or screenshot keys, are still accepted. The next save writes version 7.

```json
{
  "version": 7,
  "ui": {
    "theme": "blue",
    "windowWidth": 1024,
    "windowHeight": 768
  },
  "logging": {
    "level": "info"
  },
  "video": {
    "scaleMode": "fit",
    "integerScale": 3,
    "aspectRatio": "pixel",
    "filter": "nearest",
    "fullscreen": false,
    "vsync": true,
    "showPerformance": false,
    "brightness": 0.0,
    "contrast": 1.0,
    "saturation": 1.0,
    "gamma": 1.0,
    "sharpness": 2.0,
    "scanline": 0.5,
    "overscan": false
  },
  "audio": {
    "volume": 1.0,
    "muted": false,
    "device": "",
    "latency": "balanced"
  },
  "input": {
    "a": 29,
    "b": 27,
    "turboA": 4,
    "turboB": 22,
    "select": 229,
    "start": 40,
    "up": 82,
    "down": 81,
    "left": 80,
    "right": 79,
    "openGame": 18,
    "quickSave": 62,
    "quickLoad": 64,
    "pause": 41,
    "fastForward": 66,
    "screenshot": 69,
    "fullscreen": 40,
    "reset": 21
  },
  "library": {
    "show": true
  },
  "controllers": {
    "port1": "keyboard",
    "port2": "",
    "pad1": { "a": 1, "b": 0, "turboA": 10, "turboB": 9, "select": 4, "start": 6, "up": 11, "down": 12, "left": 13, "right": 14 },
    "pad2": { "a": 1, "b": 0, "turboA": 10, "turboB": 9, "select": 4, "start": 6, "up": 11, "down": 12, "left": 13, "right": 14 }
  },
  "emulation": {
    "region": "auto"
  }
}
```

`theme` is `blue`, `olive`, or `silver`. A file that still contains `xp-blue`, `xp-olive`, or `xp-silver` loads, and the next save writes the current name. The desktop draws its own title bar, caption buttons, and border. Window width is 640–7680 and height is 480–4320. `level` is `trace`, `debug`, `info`, `warn`, `error`, or `critical`. `scaleMode` is `fit` or `integer`. `integerScale` is 1–6. `aspectRatio` is `pixel` or `4:3`. `filter` is `nearest`, `smooth`, `sharp`, or `crt`. Brightness is −0.5–0.5, contrast is 0.5–2, saturation is 0–2, gamma is 0.4–2.5, sharpness is 1–4, and scanlines are 0–1. `overscan` crops eight pixels from each edge of the picture the window shows. `volume` is 0–1. `latency` is `short`, `balanced`, or `long`. An empty `device` selects the default playback device. Input values are key codes from 1 to 511. The ten controller keys are unique. Quick save, quick load, pause, fast forward, and screenshot are unique and are not controller keys. Open and Reset both use Ctrl, so they conflict only with each other. Fullscreen uses Alt, so it may reuse Return. `fastForward` defaults to F9 and `screenshot` defaults to F12. A controller port is `keyboard`, an empty string, or a gamepad name. Gamepad button numbers are 0–31. `turboA` and `turboB` are host keys or gamepad buttons. A turbo button is down for one emulated frame and up for the next. Holding the normal button as well keeps it down. `emulation.region` is `auto`, `ntsc`, `pal`, or `dendy`. Automatic uses the region in the cartridge header. The other three override that header and change the frame length without clearing work RAM. Settings edits that choice, and Apply is what changes the running system. Unknown fields are rejected. Do not add a setting until the application implements it.

The built-in shaders are original and compiled into the frontend. Each one has an id, a display name, a version, a stage (`sampling` or `color`), a cost, and parameters. Sampling runs before color. A compile or link failure leaves the crisp-pixel path in place. The data directory's `shaders` folder is not read.

## Game library

`library/library.json` is version 1 and is not part of the configuration file. Each game stores its path, title, detail, content id, the time it was added, and the time it was last played. The window checks whether the file is still there when it draws the list. Removing an entry does not delete the ROM. Scan walks the chosen folder and its subfolders for `*.nes` files. Directory links are not followed. The walk stops after 16 folder levels or 2000 ROM files.

## Threading

Emulation runs on a worker thread. The window copies the finished picture under the same lock used by save, load, reset, and opening a game. The worker waits until the next frame is due on `framePeriodNs()`, with one high-resolution sleep and a short pause for the last fraction of a millisecond. Each frame's samples are queued unless fast forward is on, the sound is muted, or the queue is already past the latency target. A queue that is about to run dry runs that frame early so the picture and the samples stay together. Fast forward skips the wait and does not queue audio. On Windows the process opts out of battery execution-speed limits so a light workload is not parked on a throttled core.

## Process boundary

`iris` installs a Windows crash filter and catches C++ exceptions around startup and the window. An unexpected failure is written to `logs/crash.log` and shown in a dialog. That path does not save configuration. Expected errors, such as a ROM the build cannot play, stay on the existing error dialog and leave the current game loaded. `iris --profile rom.nes` runs 300 frames without opening a window and prints the average and slowest frame time.

The desktop data root is `IRIS_DATA_DIR` when that variable is set. Otherwise Windows uses `%LOCALAPPDATA%\IrisEmulator`, then `%USERPROFILE%\AppData\Local\IrisEmulator`. Linux and macOS use `$XDG_DATA_HOME/IrisEmulator`, then `$HOME/.local/share/IrisEmulator`.
