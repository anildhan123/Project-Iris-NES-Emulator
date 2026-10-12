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
```

| Module | Responsibility | Must not depend on |
|---|---|---|
| `iris_common` | Version, errors, logging, configuration values and JSON validation | SDL, ImGui, Win32, a system core |
| `iris_platform` | Data directories, safe path joins, atomic file replacement | SDL, ImGui, UI widgets |
| `iris_emulator` | `IEmulatedSystem`, frame and audio views, controller button bits | SDL, ImGui, CPU, PPU, or mapper types |
| `iris_nes` | iNES parsing, NROM, MMC1, UxROM, CNROM, MMC3, CPU, bus, PPU, APU, controllers | SDL, ImGui, Win32 UI |
| `iris_frontend` | SDL3 window, OpenGL 3.3 context, ImGui presentation, file dialog, audio queue | CPU, PPU, APU, or mapper types |
| `iris_app` | Process startup, `--version`, optional ROM path, shutdown | Emulator internals |

The desktop calls `IEmulatedSystem`. It does not read or write CPU, PPU, or mapper state. `iris_frontend` links the interface target only. `iris_app` is what links `iris_nes`.

## Milestone 2 runtime

1. Resolve the data directory, start logging, and load configuration, as in Milestone 1.
2. Create the NES system and open the window.
3. If a ROM path was passed on the command line, or the user chooses File → Open Game, parse it as iNES or NES 2.0.
4. NROM, MMC1, UxROM, CNROM, and MMC3 run when the header sizes match that board. Anything else is reported and the previous game, if any, stays loaded.
5. While the game is running, the window calls `runFrame` once per frame period reported by the system, not once per host refresh. NTSC is about 16.64 ms. PAL and Dendy are longer. Pause skips that call.
6. The completed 256×240 RGBA picture is uploaded unchanged. A host shader samples that texture into a separate image. Nearest is the default. Smooth, sharp, and CRT sampling, then brightness, contrast, saturation, and gamma, never write back to the PPU. Save thumbnails use the PPU texture.
7. The picture is integer-scaled to fit, or to a chosen 1×–6× size. If that size does not fit, the next smaller integer is used and the status bar explains it. Pixel aspect keeps each emulated pixel square in the window; 4:3 uses a four-by-three box. Fullscreen is borderless. Samples from that frame are queued to the audio device. A missing audio device does not stop the picture.

Emulation still shares the UI thread. Moving it off that thread is a later milestone.

## NES behavior in this version

- Official 6502 opcodes, including the indirect JMP page-wrap bug. Unofficial opcodes are not implemented.
- ADC and SBC stay binary even when the decimal flag is set.
- NROM: 16 KiB PRG is mirrored at `$C000`. CHR may be ROM or RAM. A trainer, when present, is copied to `$7000`.
- UxROM switches a 16 KiB bank at `$8000` and fixes the last bank at `$C000`. Submapper 0 ANDs the written value with the ROM byte at that address. CNROM switches 8 KiB CHR banks the same way.
- MMC1 uses a five-write shift register. A second write before the next CPU cycle is ignored. MMC3 banks PRG and CHR and raises IRQ when PPU A12 rises after staying low.
- The PPU steps three dots per NTSC or Dendy CPU cycle, and 3.2 dots per PAL CPU cycle. It renders a scanline from the loopy scroll registers and raises NMI when vblank begins with NMI enabled. A12 is tracked per dot for mapper IRQ. Pattern bytes inside a dot are not fetched yet.
- NTSC is 262 scanlines and skips one dot on odd rendered frames. PAL and Dendy are 312 scanlines and do not skip that dot. The host frame period comes from `framePeriodNs()`.
- The APU mixes pulse, triangle, noise, and DMC. The frame counter follows the NTSC or PAL step sequence, including the frame IRQ. Dendy uses the NTSC APU sequence with its own CPU clock. A DMC sample read stalls the CPU for four cycles.
- Controller bits are A, B, Select, Start, Up, Down, Left, Right. Keyboard mapping lives in the shell.
- Battery-backed PRG RAM is loaded from `saves/{content id}.sav` when a game opens and written back when that game closes or the process exits. Reset leaves it in place. It is not part of a save state.

## Save states

The shell asks `IEmulatedSystem` to save, load, and inspect a state. It does not read CPU, PPU, or mapper fields.

A state file starts with the magic `IRS1`, format version 1, the NES system id, and the emulator version. It then stores a SHA-256 of the cartridge identity (mapper, submapper, region, CHR RAM size or CHR ROM bytes, and PRG ROM), the mapper id, the region, a feature-flag byte that must be zero, a timestamp, the title, an optional description, a 64×60 thumbnail, and length-delimited component chunks. A CRC32 covers every preceding byte. Chunks are `CPU `, `PPU `, `APU `, `MAP `, `CART`, `PAD `, `BUS `, and `TIME`. RAM lives inside `BUS`. An unknown chunk, a bad checksum, or a version this build does not know is rejected before the running machine changes. A failed apply restores the components captured just before the attempt.

Files live under the data directory: `states/{content id}/slot-01.irstate` through `slot-10.irstate`. Writes go to a temporary file and replace the destination only after the bytes are on disk. Reads are capped at 4 MiB. The shell keeps ten slots. Changing that count waits for the settings milestone.

## Configuration schema

Schema version 2. A version 1 file, which has no `video` object, is still accepted. The next save writes version 2 with the presentation defaults.

```json
{
  "version": 2,
  "ui": {
    "theme": "xp-blue",
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
    "scanline": 0.5
  }
}
```

`theme` is `xp-blue`, `xp-olive`, or `xp-silver`. Window width is 640–7680 and height is 480–4320. `level` is `trace`, `debug`, `info`, `warn`, `error`, or `critical`. `scaleMode` is `fit` or `integer`. `integerScale` is 1–6. `aspectRatio` is `pixel` or `4:3`. `filter` is `nearest`, `smooth`, `sharp`, or `crt`. Brightness is −0.5–0.5, contrast is 0.5–2, saturation is 0–2, gamma is 0.4–2.5, sharpness is 1–4, and scanlines are 0–1. Unknown fields are rejected. Do not add a setting until the application implements it.

The built-in shaders are original and compiled into the frontend. Each one has an id, a display name, a version, a stage (`sampling` or `color`), a cost, and parameters. Sampling runs before color. A compile or link failure leaves the crisp-pixel path in place. The data directory's `shaders` folder is not read.

## Threading

The NES still runs on the UI thread. The shell paces it with `framePeriodNs()`, which follows the cartridge region. Later milestones will keep emulation off the UI thread and will cross that boundary only through an explicit synchronization point.
