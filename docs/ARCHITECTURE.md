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
6. The completed 256×240 RGBA picture is uploaded with nearest filtering. The host does not filter or reshape PPU pixels.
7. Samples from that frame are queued to the audio device. A missing audio device does not stop the picture.

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
- Battery-backed RAM is allocated when the header asks for it and is not written to disk.

## Configuration schema

Schema version 1:

```json
{
  "version": 1,
  "ui": {
    "theme": "xp-blue",
    "windowWidth": 1024,
    "windowHeight": 768
  },
  "logging": {
    "level": "info"
  }
}
```

`theme` is `xp-blue`, `xp-olive`, or `xp-silver`. Window width is 640–7680 and height is 480–4320. `level` is `trace`, `debug`, `info`, `warn`, `error`, or `critical`. Unknown fields are rejected. Do not add a setting until the application implements it.

## Threading

The NES still runs on the UI thread. The shell paces it with `framePeriodNs()`, which follows the cartridge region. Later milestones will keep emulation off the UI thread and will cross that boundary only through an explicit synchronization point.
