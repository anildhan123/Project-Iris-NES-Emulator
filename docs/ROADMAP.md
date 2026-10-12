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

NTSC frame timing is paced on the UI thread. Dot-level PPU fetches, DMC, PAL, battery saves, and mappers other than NROM wait for later milestones.

## Milestone 3 — Accuracy and mapper expansion

PPU timing, APU completion, DMA and interrupts, more mappers, conformance tests, NTSC support, and PAL groundwork.

## Milestone 4 — Save systems

Versioned state format, quick save and load, slots, thumbnails, battery-backed RAM, and corruption handling.

## Milestone 5 — Modern graphics

Integer scaling, aspect ratio, fullscreen, shader interface, presentation filters, color settings, and performance diagnostics.

## Milestone 6 — Complete desktop frontend

Game library, recent games, settings, controller and audio configuration, XP-inspired menus and dialogs, packaging, and legal notices in the product UI.

## Milestone 7 — Compatibility and release hardening

Conformance results, regression suite, profiling, crash and error handling, a Windows release candidate, and documentation.

## Milestone 8 — Portability validation

Linux and macOS builds, a stable core API, and Android and iOS feasibility prototypes. Full mobile frontends wait until the core and state APIs are stable.
