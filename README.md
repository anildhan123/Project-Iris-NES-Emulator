# Iris Emulator

Iris Emulator is a desktop emulator platform. The first system is the Nintendo Entertainment System. This version plays NROM (mapper 0) games: it loads an iNES or NES 2.0 ROM, runs the CPU and an initial PPU, reads the keyboard as a controller, and plays pulse, triangle, and noise audio.

The project license is MIT. Third-party licenses are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Iris does not include commercial games or firmware. Iris Demo is an original program written for this project. You may use only files you are legally entitled to use.

## Requirements

- Windows 11
- Visual Studio 2026 (or another MSVC toolset with C++20), including the desktop C++ workload
- CMake 3.25 or newer
- A GPU driver that can create an OpenGL 3.3 Core context
- Git, used by CMake to fetch pinned dependencies

This machine's Visual Studio install includes CMake, but that executable is not on `PATH`:

```text
D:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
```

## Build

From the repository root, using the CMake above or any CMake 3.25+ on `PATH`:

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug --output-on-failure
.\build\windows-msvc-debug\bin\Debug\iris.exe --version
.\build\windows-msvc-debug\bin\Debug\iris.exe
```

`windows-msvc-release` is a Release build preset that reuses the same configure preset. The first configure fetches SDL, Dear ImGui, spdlog, nlohmann/json, and Catch2, then compiles them.

## What the window does

The menu contains only commands that work:

- File → Open Game, Close Game, Exit
- Emulation → Pause / Resume, Reset
- Help → About, Legal Notices

Open Game uses the system file dialog. A ROM path can also be passed on the command line. Escape pauses. The keyboard is controller 1: arrow keys, Z (A), X (B), Right Shift (Select), and Enter (Start).

The picture is the PPU's 256×240 output, scaled by whole pixels with nearest filtering. The status bar names the loaded game. Themes are `xp-blue`, `xp-olive`, and `xp-silver`, read from the configuration file.

After a build, the original demo is next to the executable:

```powershell
.\build\windows-msvc-debug\bin\Debug\iris.exe .\build\windows-msvc-debug\bin\Debug\iris_demo.nes
```

File → Open Game starts in that folder, so `iris_demo.nes` is there as well. The demo draws a tile grid and the word IRIS. The arrow keys move the sprite. Hold Z to play a tone.

Only NROM is playable. Other mappers produce an error and are not treated as NROM. Battery-backed RAM is not saved yet.

## Data directory

Configuration and logs are stored under `%LOCALAPPDATA%\IrisEmulator`, not in the install directory. Set `IRIS_DATA_DIR` to point that root somewhere else. A missing configuration file is created with defaults. A malformed file is left untouched and the session uses defaults.

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

## Documentation

- [docs/PRD.md](docs/PRD.md) — product requirements
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — module boundaries
- [docs/ROADMAP.md](docs/ROADMAP.md) — milestone status
- [docs/LEGAL_COMPLIANCE.md](docs/LEGAL_COMPLIANCE.md) — legal rules and the dependency inventory
