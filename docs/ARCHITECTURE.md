# Architecture

Iris is an emulator platform. The NES core will be the first system module. This document describes the boundaries that keep that possible. Milestone 1 implements the application shell only.

## Layers

```text
iris_app
  -> iris_frontend
       -> iris_platform -> iris_common
       -> iris_common
```

| Module | Responsibility | Must not depend on |
|---|---|---|
| `iris_common` | Version, errors, logging, configuration values and JSON validation | SDL, ImGui, Win32, a system core |
| `iris_platform` | Data directories, safe path joins, atomic file replacement | SDL, ImGui, UI widgets |
| `iris_frontend` | SDL3 window, OpenGL 3.3 context, ImGui presentation | CPU, PPU, APU, or mapper types |
| `iris_app` | Process startup, `--version`, shutdown | Emulator internals |

Later milestones add `iris_emulator` interfaces and `iris::systems::nes` behind those interfaces. The desktop UI calls application services. It does not read or write CPU, PPU, or mapper state.

## Milestone 1 runtime

1. Resolve the data directory (`IRIS_DATA_DIR`, otherwise `%LOCALAPPDATA%\IrisEmulator`).
2. Create directories for configuration, logs, states, battery saves, screenshots, shader presets, and library metadata.
3. Start logging, then load `config/settings.json`.
4. Open the window. A rejected configuration stays on disk and the process uses defaults for that session.
5. On an orderly exit after a trusted configuration, save the current window size with a temporary file and replace.

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

Milestone 1 runs on one thread. Later milestones will keep emulation off the UI thread and will cross that boundary only through an explicit synchronization point.
