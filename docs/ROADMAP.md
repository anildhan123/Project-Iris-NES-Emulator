# Roadmap

Status is updated as milestones land. Later milestones are not in progress.

## Milestone 1 — Project foundation (implemented)

- CMake build with pinned dependencies
- Logging
- Versioned JSON configuration
- SDL3 window and Windows XP-inspired shell
- Catch2 tests
- Windows CI

No emulation core is included.

## Milestone 2 — Minimal playable NES

Cartridge parser, NROM, CPU, bus, initial PPU, controller input, basic frame presentation, minimal APU, and one legally usable homebrew program.

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
