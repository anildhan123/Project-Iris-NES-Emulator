# Iris Emulator

Iris Emulator is a desktop emulator platform. The first system is the Nintendo Entertainment System. This version plays NROM, MMC1, UxROM, CNROM, and MMC3: it loads an iNES or NES 2.0 ROM, runs the CPU and PPU, reads the keyboard as a controller, and plays pulse, triangle, noise, and DMC audio. A NES 2.0 header can select PAL timing.

The project license is MIT. Third-party licenses are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). Iris does not include commercial games or firmware. Iris Demo is an original program written for this project. You may use only files you are legally entitled to use.

## Requirements

- Windows 11, or a current Ubuntu or macOS desktop
- A C++20 compiler. The Windows build uses Visual Studio 2026 (or another MSVC toolset) with the desktop C++ workload
- CMake 3.25 or newer
- A GPU driver that can create an OpenGL 3.3 Core context on Windows and Linux. macOS uses an OpenGL 3.2 compatibility context
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

```powershell
cmake --build --preset windows-msvc-release
.\build\windows-msvc-debug\bin\Release\iris.exe --version
.\build\windows-msvc-debug\bin\Release\iris.exe --profile .\build\windows-msvc-debug\bin\Release\iris_demo.nes
```

Version 0.10.0 is an unsigned build. It is not a signed installer, and the macOS app is not notarized. `cpack -G ZIP -C Release` from the Windows build directory packs the Release executable, Iris Demo, this readme, and the license files. What that build was measured to do, and what it does not claim, is in [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md).

Linux and macOS use the same desktop sources:

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
ctest --preset linux-debug --output-on-failure
./build/linux-debug/bin/iris --version
```

On macOS the preset name is `macos-debug` and the binary is `./build/macos-debug/bin/iris`. The Linux job installs a C++20 compiler, CMake, and the X11, XTEST (`libxtst-dev`), Wayland, ALSA, PulseAudio, and OpenGL packages SDL needs. A core-only tree, without the window, is:

```bash
cmake -S . -B build/core -DIRIS_DESKTOP=OFF
cmake --build build/core
ctest --test-dir build/core --output-on-failure
```

An Android NDK configure builds the JNI library instead of the window:

```bash
cmake -S . -B build/android \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24 \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DIRIS_DESKTOP=OFF
```

That library is `ports/android`. It is not an Android application. The iOS bridge is the C header `ports/ios/iris_bridge.h`.

## What the window does

The menu contains only commands that work:

- File → Open Game, Open Recent, Scan Folder, Library, Close Game, Exit
- Emulation → Pause / Resume, Reset, Save State, Load State, State Slots
- View → Scale, Aspect, Preset, Picture, Fullscreen, VSync, Performance
- Options → Settings
- Help → About, Legal Notices, Open Source Licenses

Open Game uses the system file dialog. Scan Folder adds `*.nes` files from that folder and its subfolders. A ROM path can also be passed on the command line. The library lists games you have opened or scanned, and File → Library shows or hides it. Removing a game from the library does not delete the file.

The default controls are the arrow keys, Z (A), X (B), Right Shift (Select), and Enter (Start). Numpad Enter also presses Start. Escape pauses. F5 saves the selected slot and F7 loads it. Ctrl+R resets. Shift+1 through Shift+9 select slots 1–9, and Shift+0 selects slot 10. Options → Settings can change those keys, the picture, and the audio device. State Slots shows a thumbnail, the game title, the time, and a description for each slot. Alt+Enter toggles borderless fullscreen.

The picture is the PPU's 256×240 output. Fit uses the largest whole multiple that fits. A fixed 1×–6× scale that does not fit steps down, and the status bar says so. Pixel aspect is the default; 4:3 is the television shape. Original, Smooth, Sharp, and CRT are presentation presets. Picture changes brightness, contrast, saturation, and gamma. Performance adds frame rate and timing to the status bar. None of these rewrite the emulated picture, so save thumbnails stay unfiltered. If the presentation shader cannot start, the window shows crisp pixels. The status bar names the loaded game. The window draws its own title bar, caption buttons, and border. Themes are `blue`, `olive`, and `silver`.

After a build, the original demo is next to the executable:

```powershell
.\build\windows-msvc-debug\bin\Debug\iris.exe .\build\windows-msvc-debug\bin\Debug\iris_demo.nes
```

File → Open Game starts in that folder, so `iris_demo.nes` is there as well. The demo draws a tile grid and the word IRIS. The arrow keys move the sprite. Hold Z to play a tone.

Other mappers produce an error and leave the current game loaded. There is no region menu: NTSC is the default, and PAL or Dendy comes from the NES 2.0 header. A cartridge with battery-backed RAM writes `saves/{content id}.sav` when the game closes or Iris exits, and reads it again the next time that game opens. Reset does not erase that file. Save states are separate files under `states/`.

## Data directory

Configuration, logs, the game library, save states, and battery RAM are stored outside the install directory. On Windows that is `%LOCALAPPDATA%\IrisEmulator`. On Linux and macOS it is `$XDG_DATA_HOME/IrisEmulator`, or `$HOME/.local/share/IrisEmulator` when `XDG_DATA_HOME` is unset. Set `IRIS_DATA_DIR` to point that root somewhere else. A missing configuration file is created with defaults. A version 1 or version 2 file is still read; the next save writes version 3. A malformed file is left untouched and the session uses defaults. The library file is separate, so a bad configuration does not erase it.

```json
{
  "version": 3,
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
    "scanline": 0.5
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
    "fullscreen": 40,
    "reset": 21
  },
  "library": {
    "show": true
  }
}
```

After a build, `cpack -G ZIP` from the build directory packs the executable, Iris Demo, this readme, and the license files. That zip is not a signed installer.

## Documentation

- [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md) — tested behavior and measured frame time
- [docs/PRD.md](docs/PRD.md) — product requirements
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — module boundaries
- [docs/ROADMAP.md](docs/ROADMAP.md) — milestone status
- [docs/LEGAL_COMPLIANCE.md](docs/LEGAL_COMPLIANCE.md) — legal rules and the dependency inventory
