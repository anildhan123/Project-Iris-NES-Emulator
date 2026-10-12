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

Battery-backed RAM is still not saved. Pattern bytes are not fetched dot by dot. Mappers beyond MMC3 wait for a later milestone.

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
