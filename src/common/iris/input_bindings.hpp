#pragma once

#include <optional>
#include <string>

namespace iris {

// SDL scancode numbers, stored without an SDL dependency. The shell maps them at the keyboard.
inline constexpr int kMinScancode = 1;
inline constexpr int kMaxScancode = 511;

struct InputBindings {
    int a = 29;             // Z
    int b = 27;             // X
    int turboA = 4;         // A
    int turboB = 22;        // S
    int select = 229;       // Right Shift
    int start = 40;         // Return
    int up = 82;
    int down = 81;
    int left = 80;
    int right = 79;
    int openGame = 18;      // O, used with Ctrl
    int quickSave = 62;     // F5
    int quickLoad = 64;     // F7
    int pause = 41;         // Escape
    int fastForward = 66;   // F9
    int screenshot = 69;    // F12
    int fullscreen = 40;    // Return, used with Alt
    int reset = 21;         // R, used with Ctrl

    bool operator==(const InputBindings&) const = default;
};

[[nodiscard]] inline bool validScancode(int value) {
    return value >= kMinScancode && value <= kMaxScancode;
}

// Pad keys are unique. Unmodified shortcuts are unique and are not pad keys.
// Open and Reset share the Ctrl chord, so they conflict only with each other.
// Fullscreen uses Alt, so it may reuse Return, which is also Start.
// Fast forward and screenshot are unmodified keys.
[[nodiscard]] inline std::optional<std::string> bindingConflict(const InputBindings& bindings) {
    const int pad[] = {
        bindings.a,     bindings.b,    bindings.turboA, bindings.turboB, bindings.select, bindings.start,
        bindings.up,    bindings.down, bindings.left,   bindings.right};
    const char* padNames[] = {"A", "B", "Turbo A", "Turbo B", "Select", "Start", "Up", "Down", "Left", "Right"};
    for (const int code : pad) {
        if (!validScancode(code)) {
            return "A controller key is not a valid key.";
        }
    }
    for (int i = 0; i < 10; ++i) {
        for (int j = i + 1; j < 10; ++j) {
            if (pad[i] == pad[j]) {
                return std::string(padNames[i]) + " and " + padNames[j] + " use the same key.";
            }
        }
    }

    const int plain[] = {
        bindings.quickSave, bindings.quickLoad, bindings.pause, bindings.fastForward, bindings.screenshot};
    const char* plainNames[] = {"Quick save", "Quick load", "Pause", "Fast forward", "Screenshot"};
    for (const int code : plain) {
        if (!validScancode(code)) {
            return "A shortcut is not a valid key.";
        }
    }
    if (!validScancode(bindings.openGame) || !validScancode(bindings.fullscreen) || !validScancode(bindings.reset)) {
        return "A shortcut is not a valid key.";
    }
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 10; ++j) {
            if (plain[i] == pad[j]) {
                return std::string(plainNames[i]) + " and " + padNames[j] + " use the same key.";
            }
        }
        for (int j = i + 1; j < 5; ++j) {
            if (plain[i] == plain[j]) {
                return std::string(plainNames[i]) + " and " + plainNames[j] + " use the same key.";
            }
        }
    }
    if (bindings.openGame == bindings.reset) {
        return "Open and Reset use the same key.";
    }
    return std::nullopt;
}

}  // namespace iris
