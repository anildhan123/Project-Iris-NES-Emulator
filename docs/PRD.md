# Project Iris Emulator

## Product Requirements Document (PRD), Software Requirements Specification (SRS), and Technical Design

**Document version:** 1.0  
**Project codename:** Iris Emulator  
**Initial target:** Nintendo Entertainment System (NES / Famicom)  
**Primary development environment:** Windows 11  
**Primary language:** C++20  
**Build system:** CMake  
**UI design:** Windows XP Luna-inspired classic desktop interface  
**Future platforms:** Windows, Linux, macOS, Android and iOS

---

# 1. Product Vision

Build a high-quality, accurate, modern NES emulator with a distinctive Windows XP-inspired interface, advanced graphics processing, comprehensive save-state support, customizable controls, debugging tools and a modular architecture that can later support additional gaming systems.

The application must be designed as an emulator platform rather than a monolithic NES program. The NES implementation will be the first system module.

The product must prioritize:

1. Accurate emulation of the original hardware.
2. Reliable save states and battery-backed save data.
3. Extensive optional graphics enhancements.
4. Responsive audio and input.
5. A polished, recognizable Windows XP-era desktop aesthetic.
6. Cross-platform architecture.
7. Legal handling of game files, dependencies and distributed assets.
8. Testability, maintainability and incremental development.

The objective is to build an excellent emulator, not to claim universal compatibility before it has been demonstrated through testing.

# 2. Scope

## 2.1 Initial release: NES

The first release must include:

- NES cartridge loading.
- iNES and NES 2.0 header parsing.
- CPU emulation.
- Picture Processing Unit (PPU) emulation.
- Audio Processing Unit (APU) emulation.
- Cartridge mapper architecture.
- Controller input.
- Save and load states.
- Battery-backed save RAM support.
- Configurable video scaling and filtering.
- Audio configuration.
- Keyboard and controller mapping.
- Game library and recent games.
- Windows XP-inspired interface.
- Configuration persistence.
- Logging, diagnostics and automated tests.

## 2.2 Future scope

The architecture must allow additional systems to be introduced as separate modules, potentially including Game Boy, Game Boy Color, Sega Master System, Game Gear, Mega Drive/Genesis, SNES and other systems after separate technical and legal evaluations.

Do not implement these additional systems during the initial NES milestone.

Each future system must implement a common system integration interface without forcing NES-specific behavior into the application shell.

# 3. Legal and Intellectual Property Requirements

Legal compliance is a release requirement.

## 3.1 Original implementation

- Implement the emulator core independently using publicly available technical documentation and original code.
- Do not copy proprietary emulator source code without a license that permits the intended use.
- Do not use leaked source code, proprietary SDKs or confidential hardware documentation.
- Do not copy another emulator's branding, icons, interface artwork or distinctive assets.
- Use original product branding and original UI assets.
- Do not assume that an open-source license eliminates copyright, trademark or other intellectual property obligations.

## 3.2 Game ROMs and firmware

- Do not distribute copyrighted commercial game ROMs.
- Do not bundle commercial BIOS files, proprietary firmware, copied cartridge data or unauthorized game assets.
- Do not download games automatically from ROM websites.
- Do not provide a built-in ROM piracy search engine.
- Do not bundle a copyrighted game merely because it is old or no longer sold.
- Allow users to select compatible game files that they have obtained and are legally entitled to use.
- Clearly explain that the legality of creating personal backups varies by jurisdiction and circumstance.
- Never claim that owning an original cartridge automatically makes every method of dumping, downloading or circumventing copy protection lawful.

For development and testing, use original homebrew NES programs, properly licensed test ROMs and other test materials whose licenses explicitly permit the intended use.

## 3.3 Copy protection and firmware

The emulator must not implement features intended to circumvent DRM, console security mechanisms or proprietary firmware protections.

The NES core should emulate documented hardware behavior and cartridge interfaces without requiring proprietary console firmware.

Do not implement functionality for bypassing protections on other platforms as part of this project.

## 3.4 Third-party dependencies

- Maintain a `THIRD_PARTY_NOTICES.md` file.
- Maintain a dependency inventory containing package name, version, license and source.
- Pin dependency versions.
- Review transitive dependency licenses.
- Include required notices and source-code offers where applicable.
- Avoid dependencies whose licenses conflict with the intended distribution model.
- Obtain appropriate legal review before commercial distribution.

## 3.5 Privacy and distribution

- Do not collect ROM contents, filenames or gameplay telemetry without explicit consent.
- Do not upload user files to a server.
- Do not include advertising SDKs or undisclosed analytics.
- Do not require online account registration.
- Keep game files and saves local by default.

# 4. Technology Stack

Use the following as the initial technology baseline.

| Component | Technology |
|---|---|
| Language | C++20 |
| Build system | CMake |
| Compiler support | MSVC, Clang and GCC |
| Desktop window/input/audio | SDL3 |
| Initial GPU renderer | OpenGL 3.3 Core on supported desktop systems |
| Desktop UI | Dear ImGui with a custom Windows XP-inspired theme and controls |
| Unit testing | Catch2 or GoogleTest |
| Logging | spdlog or an equivalent permissively licensed library |
| Configuration | Versioned JSON configuration |
| Save-state encoding | Versioned binary format with a documented schema |
| Static analysis | Compiler warnings, clang-tidy and sanitizers where supported |
| Continuous integration | Automated builds and tests on supported operating systems |

Before committing to dependencies, verify current supported versions, platform coverage and licensing.

Keep all third-party code isolated behind appropriate wrappers.

The initial Windows desktop application must be the first complete frontend. The core must not depend on Windows APIs, ImGui or SDL window objects.

For future mobile support, define a stable C-compatible core API and separate platform integration layers. SDL3 can be evaluated for mobile input, audio and rendering integration, but do not assume desktop OpenGL support maps directly to iOS. Provide an abstraction that can use Metal or another supported graphics backend when needed.

# 5. System Architecture

Use a layered architecture.

## 5.1 Layer A: Shared application infrastructure

Responsibilities:

- Application lifecycle.
- Logging.
- Configuration management.
- File-system abstractions.
- Error reporting.
- Resource management.
- Platform detection.
- Version information.
- Localization infrastructure.
- Update checks only if explicitly enabled in a future release.

## 5.2 Layer B: Emulator framework

Define system-independent interfaces for:

- System creation and destruction.
- ROM or content loading.
- Reset and shutdown.
- Emulation execution.
- Pause and resume.
- Frame output.
- Audio output.
- Input delivery.
- Save-state creation and restoration.
- Battery-backed save data.
- System metadata.
- Capability reporting.

Illustrative interface:

```cpp
class IEmulatedSystem {
public:
    virtual ~IEmulatedSystem() = default;

    virtual bool loadContent(const ContentSource& source) = 0;
    virtual void reset() = 0;
    virtual void runFrame() = 0;
    virtual void setPaused(bool paused) = 0;

    virtual bool saveState(StateWriter& writer) const = 0;
    virtual bool loadState(StateReader& reader) = 0;

    virtual SystemInfo getSystemInfo() const = 0;
};
```

This interface is illustrative. Adapt it to the actual implementation and avoid unnecessary abstractions that increase latency.

The core must not directly manipulate desktop UI widgets.

## 5.3 Layer C: NES system module

Create an independent NES module containing:

- CPU.
- Bus.
- PPU.
- APU.
- Cartridge.
- Mapper registry.
- Controller ports.
- Timing coordinator.
- Region configuration.
- NES-specific state serialization.
- NES diagnostics.

Suggested namespace: `iris::systems::nes`.

Do not expose NES memory addresses or mapper-specific concepts to the general UI unless a diagnostic tool explicitly requests them.

## 5.4 Layer D: Rendering

Rendering must be separate from emulated hardware.

The PPU produces the emulated picture. A host-side video pipeline then transforms and presents that picture.

Responsibilities:

- Uploading emulated frame data.
- Scaling.
- Aspect ratio handling.
- Shader execution.
- Optional color correction.
- Presentation synchronization.
- Fullscreen.
- Renderer capability reporting.
- Graphics settings validation.

## 5.5 Layer E: Audio

Responsibilities:

- Receiving audio samples from the emulated APU.
- Resampling to the host audio rate.
- Audio buffering.
- Output device selection.
- Volume and mute.
- Optional host-side audio effects.
- Audio latency configuration.

Host-side audio effects must not modify the emulated APU's internal state.

## 5.6 Layer F: Input

Responsibilities:

- Keyboard mapping.
- Gamepad mapping.
- Multiple controller ports.
- Controller hot-plugging.
- Analog-to-digital mapping where applicable.
- Configurable hotkeys.
- Input focus handling.
- Touch input for mobile frontends.

## 5.7 Layer G: Frontend

The frontend includes:

- Main window.
- Game library.
- Menus.
- Toolbars.
- Settings.
- Game display.
- Save-state manager.
- Controller configuration.
- Diagnostics.
- Optional debugging tools.

The frontend must call application services rather than reaching into the CPU, PPU or mapper implementation directly.

# 6. Repository Structure

Use a clean, modular repository.

```text
iris-emulator/
├── CMakeLists.txt
├── CMakePresets.json
├── README.md
├── LICENSE
├── THIRD_PARTY_NOTICES.md
├── docs/
│   ├── PRD.md
│   ├── ARCHITECTURE.md
│   ├── NES_IMPLEMENTATION.md
│   ├── STATE_FORMAT.md
│   ├── GRAPHICS_PIPELINE.md
│   ├── LEGAL_COMPLIANCE.md
│   └── ROADMAP.md
├── assets/
│   ├── fonts/
│   ├── icons/
│   └── themes/
├── config/
├── src/
│   ├── app/
│   ├── common/
│   ├── platform/
│   ├── frontend/
│   │   ├── desktop/
│   │   └── ui/
│   ├── rendering/
│   │   ├── interfaces/
│   │   ├── opengl/
│   │   └── shaders/
│   ├── audio/
│   ├── input/
│   ├── content/
│   ├── save/
│   ├── library/
│   ├── emulator/
│   │   ├── interfaces/
│   │   └── state/
│   └── systems/
│       └── nes/
│           ├── cpu/
│           ├── bus/
│           ├── ppu/
│           ├── apu/
│           ├── cartridge/
│           ├── mappers/
│           ├── input/
│           ├── timing/
│           └── tests/
├── tests/
│   ├── unit/
│   ├── integration/
│   ├── compatibility/
│   └── testdata/
├── tools/
└── packaging/
    ├── windows/
    ├── linux/
    └── macos/
```

Add Android and iOS project directories when mobile implementation begins.

Do not commit commercial ROMs, private save data, generated build output or unlicensed assets.

# 7. NES Emulation Requirements

## 7.1 CPU

Implement the NES 2A03/2A07 CPU behavior, based on the 6502 family with NES-specific differences.

Requirements:

- Registers A, X, Y, PC, SP and status flags.
- Instruction decoding and execution.
- Correct addressing modes.
- Official opcodes.
- Unofficial opcodes where needed for compatibility.
- Interrupt handling: IRQ, NMI and reset.
- Stack operations.
- Branch timing.
- Page-crossing cycle penalties.
- DMA interactions.
- Memory-mapped I/O.
- Correct interaction with the PPU and APU.
- Deterministic CPU state serialization.

Do not treat instruction-level correctness alone as proof of cycle accuracy.

Begin with an instruction-level implementation that has explicit cycle accounting. Refine it toward the timing model required by compatibility tests.

## 7.2 Memory bus

Implement:

- 2 KiB internal CPU RAM and mirrors.
- PPU register mapping and mirrors.
- APU register access.
- Controller ports.
- OAM DMA.
- Cartridge PRG-ROM and PRG-RAM access.
- Mapper-controlled banking.
- Open-bus behavior where required.
- Correct memory access side effects.

Use centralized bus logic so memory mapping is not duplicated across CPU instructions.

## 7.3 PPU

Implement the picture processing unit, including:

- Background rendering.
- Sprite rendering.
- Pattern tables.
- Nametables and attribute tables.
- Palette RAM.
- OAM.
- Horizontal and vertical scrolling.
- Sprite priority.
- Sprite-zero hit.
- Sprite overflow behavior.
- PPU register side effects.
- VBlank and NMI timing.
- Scanline and dot progression.
- Rendering-disabled behavior.
- PPU memory mirroring.
- NTSC and PAL differences.
- Mapper interaction during rendering.

The normal visible picture is 256 × 240 pixels.

The rendering implementation must preserve correct sprite priority, scrolling, mid-frame register changes and timing-sensitive effects. Avoid implementing the PPU solely as a static tilemap renderer.

Expose a stable completed-frame buffer to the host renderer.

## 7.4 APU

Implement the standard NES audio channels:

- Pulse channel 1.
- Pulse channel 2.
- Triangle.
- Noise.
- DMC.

Requirements:

- Envelopes.
- Length counters.
- Sweep units.
- Linear counter.
- Noise generation.
- DMC sample playback and DMA behavior.
- Frame counter.
- IRQ behavior where applicable.
- Correct register side effects.
- Channel mixing.
- Sample generation.
- Deterministic APU state serialization.

Plan a clean extension interface for cartridge expansion audio, but do not require every expansion audio device in the first release.

## 7.5 Cartridge parsing

Support:

- iNES format.
- NES 2.0 format.
- Header validation.
- PRG-ROM and CHR-ROM.
- CHR-RAM.
- PRG-RAM and nonvolatile PRG memory.
- Trainer handling.
- Mirroring configuration.
- Region and console metadata where available.
- Unsupported mapper detection.
- Malformed-file rejection.
- File-size and memory-allocation limits.

Never silently interpret invalid headers as a supported game.

Display clear error messages for unsupported formats and mappers.

## 7.6 Mapper architecture

Implement a registry and common mapper interface.

Initial priority:

1. NROM / Mapper 0.
2. UxROM / Mapper 2.
3. CNROM / Mapper 3.
4. MMC1 / Mapper 1.
5. MMC3 / Mapper 4.

Implement additional mappers based on compatibility demand and available test coverage.

Mapper responsibilities must include banking, mirroring changes, cartridge RAM and relevant IRQ behavior.

Mapper-specific timing must be supported without hardcoding mapper conditions throughout the CPU and PPU.

## 7.7 Controller emulation

Initial support:

- Standard NES controller in port 1.
- Standard NES controller in port 2.
- Keyboard input.
- USB and Bluetooth gamepads where the host platform supports them.
- Hot-plug detection.
- Configurable button mapping.
- Turbo buttons as an optional host-side feature.
- Input latency controls only when they can be implemented reliably.

Keep controller state serializable.

# 8. Emulation Timing and Execution

Use a deterministic emulation scheduler.

The CPU, PPU, APU, DMA and mapper events must remain synchronized according to the selected console region.

Requirements:

- NTSC support first.
- PAL support as a planned milestone.
- Explicit region selection and automatic selection where reliable metadata exists.
- Frame-boundary detection.
- Stable execution without reliance on host frame rate.
- No arbitrary frame skipping during normal operation.
- Optional frame skipping under user control.
- Fast-forward.
- Pause.
- Single-frame stepping for debugging.
- Reset.
- Accurate interrupt scheduling.

Use rational or integer-based timing accumulators where practical to avoid long-term floating-point drift.

The emulator must not assume that every game runs at the same exact frame rate on every region.

# 9. Save and Load State

Save states are a core feature and must not be treated as a late UI addition.

## 9.1 Quick states

Provide:

- Quick Save.
- Quick Load.
- At least 10 configurable numbered slots.
- Slot selection from the menu.
- Keyboard shortcuts.
- Thumbnail previews.
- Timestamp.
- Game title and content identifier.
- Optional user description.

Allow the number of slots to be increased through settings.

## 9.2 State contents

Serialize all state required to resume execution deterministically, including:

- CPU registers and internal execution state.
- CPU RAM.
- PPU registers and internal pipeline state.
- PPU memory and OAM.
- APU channel state and frame sequencer.
- Mapper registers and banking state.
- Cartridge RAM.
- Controller shift-register state.
- DMA state.
- Master timing state.
- Interrupt lines and pending events.
- Audio generation state.
- Any additional state required by the implementation.

Do not save only visible RAM and CPU registers. That would produce incorrect resumption in many games.

## 9.3 File format

Use a versioned, documented state format.

Required metadata:

- Magic signature.
- Format version.
- System identifier.
- Emulator build/version.
- Content hash.
- Mapper identifier.
- Region.
- Required feature flags.
- Serialized component data.
- Integrity checksum.

Use length-delimited sections where practical.

Reject malformed, truncated, incompatible or suspiciously large state files safely.

Do not execute code from save states.

## 9.4 Compatibility and safety

- Write states atomically using a temporary file and rename.
- Validate all lengths before allocation.
- Validate content identity before loading.
- Keep the running emulator unchanged if a load fails.
- Display a useful error.
- Never overwrite a valid state with a failed or partial write.
- Document state-version compatibility.
- Support state migration only when explicitly implemented and tested.

The initial release does not need to guarantee compatibility between every future emulator version.

## 9.5 Battery-backed save data

Implement cartridge battery-backed RAM persistence separately from save states.

- Load existing save RAM when opening a game.
- Save changes when appropriate.
- Use safe file replacement.
- Flush on orderly shutdown.
- Provide a best-effort flush when switching games.
- Avoid writing on every frame.
- Handle disk-full and permission errors.
- Clearly distinguish battery saves from emulator states.

# 10. Modern Graphics Enhancement System

The graphics pipeline must be independent of NES hardware emulation.

The original NES output remains 256 × 240 pixels. Higher output resolution increases presentation quality but does not create additional original game detail.

## 10.1 Scaling modes

Support:

- Native 1×.
- Integer 2×.
- Integer 3×.
- Integer 4×.
- Integer 5× and 6× where useful.
- Fit to window.
- Fit to screen.
- Custom scale.
- Fullscreen.
- Borderless fullscreen where supported.
- Window resizing.
- Aspect ratio preservation.
- Optional overscan cropping.
- Optional custom viewport.

At integer 2×, the nominal picture becomes 512 × 480; at 3×, 768 × 720; at 4×, 1024 × 960.

The frontend must account for the available display size, DPI and aspect ratio. If the requested integer scale does not fit, explain the fallback rather than distorting the picture.

## 10.2 Pixel presentation filters

Provide selectable presentation modes:

- Nearest-neighbor / crisp pixels.
- Linear interpolation.
- Sharp bilinear filtering.
- Optional scale-dependent sharpening.
- CRT-inspired scanlines.
- CRT mask or grille simulation.
- Phosphor-inspired effects.
- Curvature and vignette.
- Composite-video-inspired filtering.
- Optional NTSC color and artifact simulation.
- LCD pixel-grid effects.
- Optional handheld-inspired display presets for future handheld systems.

These are post-processing options, not requirements for changing NES hardware emulation.

## 10.3 Advanced shader pipeline

Build a modular shader chain.

Each effect must have:

- A unique identifier.
- A display name.
- A version.
- Parameters with validated ranges.
- An enabled/disabled state.
- A documented processing stage.
- A performance cost estimate where feasible.

Support effect ordering when technically safe.

Provide presets such as:

- Original / Pixel Perfect.
- Smooth.
- CRT.
- Sharp CRT.
- Composite.
- Custom.

Shader failures must fall back to a safe renderer or disable the failing effect without crashing the application.

Only bundle shaders whose licenses permit redistribution.

## 10.4 Anti-aliasing

Provide a clearly explained graphics setting for optional smoothing and anti-aliasing-style post-processing.

Important constraints:

- Traditional MSAA and similar geometric anti-aliasing techniques do not meaningfully improve the original flat pixel-art image in the same way they improve polygonal 3D graphics.
- Pixel-art smoothing may blur intended edges.
- Do not claim that enabling anti-aliasing improves hardware emulation accuracy.
- Offer nearest-neighbor as the default for authentic pixel presentation.
- Keep any optional smoothing filter reversible.

If temporal or edge-aware smoothing is added later, implement it as an optional host-side effect with explicit handling for frame history and save-state restoration.

## 10.5 Resolution and aspect ratio

- Preserve the source aspect ratio by default.
- Provide an integer-scale option.
- Provide a user-controlled aspect-ratio override.
- Avoid unintended stretching.
- Support high-DPI displays.
- Separate logical UI size from the game viewport.
- Allow fullscreen and windowed modes.
- Recalculate the viewport after resize and display changes.

## 10.6 Color settings

Optional settings:

- Brightness.
- Contrast.
- Saturation.
- Gamma.
- Color palette presets.
- Color temperature if practical.
- NTSC/PAL-related display presets.

Maintain the original color pipeline as a selectable baseline.

## 10.7 Future high-quality rendering

Design extension points for future systems with higher native resolutions, polygonal graphics or 3D rendering.

Do not implement fake 3D upscaling or AI-based enhancement for NES in the initial release. Such features should be separate, optional projects with explicit performance and compatibility requirements.

# 11. Audio Features

Required:

- Master volume.
- Mute.
- Output-device selection where supported.
- Configurable audio buffer size or latency preset.
- Sample-rate conversion.
- Optional channel mixing adjustments for diagnostics.
- Optional audio recording to a documented format if licensing and implementation permit.
- Audio-device recovery after disconnect.
- No audio callback blocking on file I/O or UI operations.

The original APU output must remain the reference mode.

Any equalizer, reverb or other enhancement must be an optional host-side effect.

# 12. User Interface: Windows XP Style

The desktop frontend should feel like a polished application from the Windows XP Luna era, not a modern flat web dashboard.

## 12.1 Visual identity

Use:

- Classic blue title bars.
- Tahoma typography.
- Raised and sunken bevels.
- Compact toolbars.
- Classic menu bars.
- Small, recognizable toolbar icons.
- Blue selection highlights.
- Light gray dialog backgrounds.
- Familiar tabs, checkboxes and dropdowns.
- A conventional status bar.
- Optional Luna-inspired blue, olive and silver themes.

The application must use original icons and assets rather than copying proprietary Windows system files.

## 12.2 Main window

Suggested layout:

1. Title bar: Iris Emulator.
2. Menu bar.
3. Toolbar.
4. Optional game-library sidebar.
5. Main game viewport.
6. Status bar.

The viewport should receive the majority of the available space.

The UI must not impose excessive padding or modern rounded-card styling.

## 12.3 Menus

**File**

- Open Game
- Open Recent
- Open Game Folder
- Close Game
- Save State
- Load State
- Exit

**Emulation**

- Pause / Resume
- Reset
- Soft Reset, where supported
- Stop
- Frame Advance
- Fast Forward
- Region

**Video**

- Fullscreen
- Integer Scaling
- Scale
- Aspect Ratio
- Filters
- Shader Presets
- Overscan
- Screenshot

**Audio**

- Mute
- Volume
- Output Device
- Latency

**Input**

- Configure Controller
- Configure Hotkeys
- Controller Port Assignment

**Tools**

- Save-State Manager
- Game Information
- Debugger, when available
- Performance Statistics
- Logs

**Options**

- Settings
- Theme
- Paths
- Configuration Profiles

**Help**

- About
- Documentation
- Legal Notices
- Open Source Licenses

Menu items must reflect actual capabilities. Do not display nonfunctional controls.

## 12.4 Game library

Support:

- Add game file.
- Scan a user-selected directory.
- Recently played games.
- Search by title or filename.
- Sort by title and last played.
- Display filename and supported metadata.
- Optional user-generated cover art.
- Missing-file detection.
- Remove entries from the library without deleting source files.

Do not scrape or download copyrighted cover art automatically.

## 12.5 Settings dialog

Use tabbed settings:

- General.
- Video.
- Audio.
- Input.
- Emulation.
- Paths.
- Advanced.
- About.

Changes should be validated before being committed.

Provide Apply, OK and Cancel behavior where practical.

Provide Reset to Defaults with a confirmation dialog.

## 12.6 Keyboard navigation and accessibility

- Support keyboard navigation throughout the UI.
- Make menu actions discoverable.
- Provide tooltips for graphics settings.
- Maintain readable contrast.
- Support high-DPI scaling.
- Keep the game viewport usable at small window sizes.
- Provide configurable shortcuts.
- Detect conflicting hotkeys.

## 12.7 Window XP appearance without legacy limitations

The visual theme must be independent of the operating system version.

Do not depend on actual Windows XP APIs or use unsupported legacy APIs.

The app must run on supported modern Windows releases and must remain usable on Linux and macOS with platform-appropriate window decorations.

# 13. Input and Hotkey Requirements

Default keyboard mapping:

- Arrow keys: D-pad.
- Z: A.
- X: B.
- Right Shift: Select.
- Enter: Start.
- Escape: pause or open an emulation menu, depending on configuration.

Default application shortcuts:

- Ctrl+O: Open Game.
- F5: Quick Save.
- F8: Quick Load.
- F9: Toggle Fast Forward.
- F10: Pause/Resume.
- F11: Fullscreen.
- Ctrl+R: Reset.
- F12: Screenshot.

These defaults must be configurable.

Prevent accidental quick-load and destructive actions where practical.

Input processing must remain responsive during emulation, pause, focus changes and controller disconnects.

# 14. Application Data and File Management

Use platform-specific application data directories.

Keep separate directories for:

- Configuration.
- Save states.
- Battery saves.
- Screenshots.
- Logs.
- Shader presets.
- Game library metadata.

Never place user data inside the installation directory by default.

Provide a settings screen to view or change supported paths.

Store configuration in a versioned format.

Requirements:

- Validate configuration before loading.
- Recover from malformed configuration using documented defaults.
- Preserve user data during upgrades.
- Do not delete ROMs when removing library entries.
- Use safe file names.
- Avoid path traversal.
- Handle Unicode paths.
- Handle removable drives.
- Display actionable file errors.

# 15. Performance Requirements

These are initial engineering targets, not claims of already demonstrated performance.

- Run common NES games at their intended speed on a typical modern desktop.
- Avoid unbounded allocations in the per-frame hot path.
- Avoid unnecessary frame-buffer copies.
- Avoid blocking file operations in audio callbacks.
- Keep rendering independent from emulation timing.
- Prevent UI rendering from stalling the emulation thread.
- Support frame pacing and configurable synchronization.
- Provide optional FPS and timing statistics.
- Maintain bounded audio buffers.
- Avoid busy-wait loops that waste CPU when paused.

Use profiling before introducing complicated optimization techniques.

Do not sacrifice correctness for performance without a measured, documented reason.

# 16. Threading Model

Begin with a simple execution model.

Recommended initial design:

- Main/UI thread: event processing and UI.
- Emulation thread: CPU, PPU, APU, mapper and frame execution.
- Audio callback or audio subsystem: consumes prepared audio samples.
- Background tasks: library scanning and file operations.

Use bounded queues or equivalent safe handoff mechanisms for audio and video.

Define synchronization ownership for all shared state.

Do not allow the UI to directly mutate CPU or PPU internals from another thread.

Pause, reset, state load and state save must use a well-defined synchronization boundary.

For the initial release, it is acceptable to serialize emulator state at a safe frame boundary if the resulting behavior is correct and responsive.

# 17. Debugging and Diagnostics

Provide an internal diagnostic framework.

Initial capabilities:

- Log levels.
- File logging.
- Runtime error messages.
- CPU instruction tracing in development builds.
- CPU register inspection.
- Memory viewer.
- Disassembler.
- PPU register inspection.
- Frame timing statistics.
- Mapper identification.
- Content hash display.
- Save-state validation errors.

Debugger interfaces must be optional and must not impose a significant performance penalty in normal release builds.

Use development-only flags to enable expensive tracing.

# 18. Testing Strategy

Testing must begin before the first complete game is playable.

## 18.1 Unit tests

Test:

- CPU instructions and addressing modes.
- Flags and arithmetic.
- Stack behavior.
- Branch timing.
- Interrupts.
- Bus mapping.
- RAM mirroring.
- PPU register behavior.
- Palette behavior.
- Sprite priority.
- Controller shift registers.
- APU channel state.
- Mapper bank switching.
- Cartridge parsing.
- Save-state serialization.
- Configuration validation.
- Error handling.

## 18.2 Integration tests

Test:

- Loading a valid homebrew ROM.
- Rejecting malformed ROM files.
- Starting and resetting a game.
- Controller input.
- Audio generation.
- Video output.
- Saving and loading states.
- Battery save persistence.
- Switching games.
- Renderer failure and recovery.
- Missing files and permission errors.

## 18.3 Conformance testing

Use appropriately licensed NES test ROMs, including CPU and PPU test suites where licensing permits.

Track each test's expected result.

Do not claim full NES compatibility based on a handful of popular games.

## 18.4 Regression testing

Maintain a curated set of legally usable test programs.

For deterministic test cases, compare:

- CPU state.
- Memory snapshots.
- Frame hashes.
- Audio sample hashes or numerical tolerances.
- Save-state round-trip results.

Document expected differences between platforms and backends.

## 18.5 Save-state tests

For each supported test program:

1. Run to a deterministic checkpoint.
2. Save the state.
3. Continue and record a reference result.
4. Reload the saved state.
5. Continue for the same interval.
6. Compare deterministic state and output.
7. Verify that corrupted states are rejected safely.

## 18.6 Continuous integration

Build and test on Windows first.

Add Linux and macOS CI when their build configuration is stable.

Add Android and iOS build validation during mobile implementation.

Enable compiler warnings and treat newly introduced serious warnings as defects.

# 19. Security and Robustness

ROMs and state files are untrusted input.

Requirements:

- Validate headers and file sizes.
- Guard integer arithmetic.
- Avoid unchecked memory allocation.
- Reject malformed data.
- Prevent out-of-bounds access.
- Validate save-state chunk lengths.
- Avoid unsafe deserialization.
- Do not execute file content.
- Limit allocations according to validated format constraints.
- Provide meaningful error messages.
- Use sanitizers during development.
- Do not include remote code execution or script execution from game files.

A malformed file must not crash the application or corrupt unrelated user data.

# 20. Configuration Model

Create a centralized configuration service.

Example conceptual settings:

```json
{
  "version": 1,
  "video": {
    "scaleMode": "integer",
    "integerScale": 3,
    "aspectRatio": "preserve",
    "filter": "nearest",
    "shaderPreset": "original",
    "fullscreen": false,
    "vsync": true
  },
  "audio": {
    "enabled": true,
    "volume": 1.0,
    "latencyPreset": "balanced"
  },
  "emulation": {
    "region": "auto",
    "runAheadFrames": 0,
    "rewindEnabled": false
  },
  "ui": {
    "theme": "blue",
    "showLibrarySidebar": true
  }
}
```

This is a conceptual schema, not a mandatory complete configuration file.

Every setting must have:

- A documented default.
- Validated allowed values.
- A clear effect on the application.
- A persistence policy.
- A migration strategy when the schema changes.

Do not add settings that are not implemented.

# 21. Future Features

The following are planned extensions, not first-release requirements:

- Rewind.
- Run-ahead input-latency reduction.
- RetroAchievements integration, subject to service terms and legal review.
- Netplay.
- Rollback networking.
- Replay recording.
- Movie playback.
- Game-specific profiles.
- Cloud synchronization with explicit consent.
- Mobile touch controls.
- Multiple emulator cores.
- Additional consoles.
- Optional advanced video shaders.
- Accessibility overlays.

Each extension must have its own requirements, tests and performance criteria.

Rewind requires periodic in-memory snapshots and deterministic state restoration.

Run-ahead must be disabled by default and must be evaluated for game compatibility and CPU cost.

Netplay and rollback should not be started until deterministic execution and state serialization are sufficiently reliable.

# 22. Cross-Platform Design

## Windows

- Primary development platform.
- Native window integration through SDL3.
- XP-inspired desktop UI.
- Windows packaging and configuration paths.
- Controller and audio-device integration.

## Linux

- CMake-based build.
- SDL3 platform support.
- User data in standard platform-specific directories.
- Test on at least one mainstream desktop distribution.
- Document graphics-driver requirements.

## macOS

- CMake-based native build.
- Supported graphics backend.
- Native application packaging.
- Keyboard and controller support.
- Test Apple Silicon builds.
- Validate signing and notarization requirements for distributed builds.

## Android

- Reuse the C++ core.
- Integrate through the Android NDK and JNI or another well-defined native bridge.
- Support lifecycle suspension and resumption.
- Handle audio focus.
- Support touch controls and controllers.
- Manage scoped storage correctly.
- Do not assume that a game remains active while the app is backgrounded.

## iOS

- Reuse the C++ core.
- Integrate through Objective-C++ or a stable C-compatible interface.
- Use a graphics backend compatible with supported Apple devices.
- Handle app suspension and audio interruptions.
- Support touch controls and compatible game controllers.
- Package and distribute according to current Apple platform requirements.

Cross-platform portability means sharing the core and contracts. It does not require every frontend to use identical controls or platform APIs.

# 23. Release Acceptance Criteria

The initial release is acceptable only when all mandatory criteria pass.

### Emulation

- The CPU passes the selected mandatory conformance tests.
- PPU timing and rendering tests meet documented expectations.
- Supported cartridge formats load correctly.
- Unsupported mappers are reported clearly.
- Controller input works.
- Audio remains stable during normal execution.
- Reset and pause behave consistently.

### Save systems

- Quick save and load work.
- Multiple slots work.
- State identity is validated.
- Invalid states are rejected safely.
- Battery saves persist across application restarts.
- A failed load does not leave the running emulator in a partially restored state.

### Graphics

- Integer scaling works at supported sizes.
- Aspect ratio is preserved by default.
- Fullscreen and windowed modes work.
- Filters can be enabled and disabled.
- Unsupported shaders fail gracefully.
- Graphics settings survive restarts.
- Original pixel presentation remains available.

### UI

- Menus and dialogs follow the XP-inspired visual specification.
- The UI remains responsive during gameplay.
- Keyboard navigation works.
- Hotkeys are configurable.
- Errors are understandable.
- High-DPI displays are supported.
- No menu action is presented as functional unless implemented.

### Portability

- The NES core builds without depending on desktop UI classes.
- The build uses CMake.
- Platform-specific code is isolated.
- The architecture supports a second system module without rewriting the NES core.
- A documented C-compatible interface is available for future mobile integration.

### Legal compliance

- No unauthorized commercial ROMs are bundled.
- All distributed dependencies have been reviewed.
- Required notices are present.
- All bundled artwork, fonts and shaders have appropriate distribution rights.
- No unauthorized firmware or DRM circumvention is included.

# 24. Implementation Roadmap

## Milestone 1 — Project foundation

Deliver:

- Repository structure.
- CMake build.
- Logging.
- Configuration service.
- SDL3 window.
- Basic XP-inspired UI shell.
- Automated test framework.
- CI build for Windows.

Do not implement the entire emulator in this milestone.

## Milestone 2 — Minimal playable NES

Deliver:

- Cartridge parser.
- NROM mapper.
- CPU.
- Bus.
- Initial PPU.
- Controller input.
- Basic frame presentation.
- Minimal APU.
- One legally usable homebrew test program.

## Milestone 3 — Accuracy and mapper expansion

Deliver:

- PPU timing improvements.
- APU completion.
- DMA and interrupt handling.
- Additional mappers.
- Conformance tests.
- NTSC support and PAL groundwork.

## Milestone 4 — Save systems

Deliver:

- Versioned state format.
- Quick save/load.
- Multiple slots.
- Thumbnails.
- Battery-backed RAM persistence.
- Corruption handling.

## Milestone 5 — Modern graphics

Deliver:

- Integer scaling.
- Aspect ratio modes.
- Fullscreen.
- Shader interface.
- Nearest, bilinear and CRT-style filters.
- Color settings.
- Performance diagnostics.

## Milestone 6 — Complete desktop frontend

Deliver:

- Game library.
- Recent games.
- Settings.
- Controller configuration.
- Audio configuration.
- XP-inspired menus and dialogs.
- Packaging.
- Legal notices.

## Milestone 7 — Compatibility and release hardening

Deliver:

- Conformance results.
- Regression suite.
- Performance profiling.
- Crash and error handling.
- Windows release candidate.
- Documentation.

## Milestone 8 — Portability validation

Deliver:

- Linux build.
- macOS build.
- Stable core API.
- Android feasibility prototype.
- iOS feasibility prototype on macOS/Xcode infrastructure.

Do not start full mobile frontend development until the core and state APIs are stable.

# 25. Cursor AI Development Rules

Cursor must follow these rules throughout development.

1. Read the project documentation before modifying the architecture.
2. Implement one milestone at a time.
3. Do not generate the entire project in one massive change.
4. Keep each module independently testable.
5. Compile after each meaningful change.
6. Run relevant unit and integration tests.
7. Fix existing regressions before adding more features.
8. Do not silently replace libraries or alter dependency licenses.
9. Do not introduce proprietary or unauthorized assets.
10. Do not add copyrighted game ROMs or firmware.
11. Do not implement nonfunctional placeholder controls in a release build.
12. Do not make claims about compatibility that have not been tested.
13. Do not sacrifice deterministic emulation for UI convenience.
14. Do not let rendering filters change the emulated PPU output.
15. Do not access CPU or PPU internals directly from UI code.
16. Do not add speculative abstractions without a clear architectural purpose.
17. Document important design decisions.
18. Keep the application buildable at the end of each milestone.
19. Update the roadmap and implementation status as work progresses.
20. Prefer small, reviewable changes over large rewrites.

# 26. Definition of Success

Project Iris Emulator succeeds when it provides an accurate, reliable and enjoyable NES experience with modern conveniences, an attractive Windows XP-inspired interface, extensible graphics processing, dependable save systems and a codebase that can support additional systems and platforms.

Quality must be demonstrated through conformance tests, regression tests, performance measurements and documented platform builds rather than marketing claims.

The first objective is a correct and maintainable NES emulator. Advanced graphics, additional systems and mobile releases must build upon that foundation.
