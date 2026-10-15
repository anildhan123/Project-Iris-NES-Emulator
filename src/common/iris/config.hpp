#pragma once

#include "iris/error.hpp"
#include "iris/input_bindings.hpp"
#include "iris/presentation.hpp"
#include "iris/region_choice.hpp"

#include <string>
#include <string_view>

namespace iris {

inline constexpr int kMinWindowWidth = 640;
inline constexpr int kMinWindowHeight = 480;
inline constexpr int kMaxWindowWidth = 7680;
inline constexpr int kMaxWindowHeight = 4320;

enum class Theme {
    Blue,
    Olive,
    Silver,
};

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
};

enum class AudioLatency {
    Short,
    Balanced,
    Long,
};

struct GamepadMap {
    int a = 1;
    int b = 0;
    int turboA = 10;
    int turboB = 9;
    int select = 4;
    int start = 6;
    int up = 11;
    int down = 12;
    int left = 13;
    int right = 14;

    bool operator==(const GamepadMap&) const = default;
};

struct ControllerSetup {
    std::string port1 = "keyboard";
    std::string port2;
    GamepadMap pad1{};
    GamepadMap pad2{};

    bool operator==(const ControllerSetup&) const = default;
};

struct Config {
    static constexpr int kCurrentVersion = 7;

    int version = kCurrentVersion;
    Theme theme = Theme::Blue;
    int windowWidth = 1024;
    int windowHeight = 768;
    LogLevel logLevel = LogLevel::Info;
    ScaleMode scaleMode = ScaleMode::Fit;
    int integerScale = 3;
    AspectMode aspect = AspectMode::Pixel;
    PictureFilter filter = PictureFilter::Nearest;
    bool fullscreen = false;
    bool vsync = true;
    bool showPerformance = false;
    float brightness = kBrightness.defaultValue;
    float contrast = kContrast.defaultValue;
    float saturation = kSaturation.defaultValue;
    float gamma = kGamma.defaultValue;
    float sharpness = kSharpness.defaultValue;
    float scanline = kScanline.defaultValue;
    float volume = 1.0f;
    bool muted = false;
    std::string audioDevice;
    AudioLatency audioLatency = AudioLatency::Balanced;
    InputBindings input{};
    bool showLibrary = true;
    bool overscan = false;
    ControllerSetup controllers{};
    RegionChoice region = RegionChoice::Automatic;
};

[[nodiscard]] inline bool operator==(const Config& left, const Config& right) {
    return left.version == right.version && left.theme == right.theme && left.windowWidth == right.windowWidth &&
           left.windowHeight == right.windowHeight && left.logLevel == right.logLevel &&
           left.scaleMode == right.scaleMode && left.integerScale == right.integerScale && left.aspect == right.aspect &&
           left.filter == right.filter && left.fullscreen == right.fullscreen && left.vsync == right.vsync &&
           left.showPerformance == right.showPerformance && left.brightness == right.brightness &&
           left.contrast == right.contrast && left.saturation == right.saturation && left.gamma == right.gamma &&
           left.sharpness == right.sharpness && left.scanline == right.scanline && left.volume == right.volume &&
           left.muted == right.muted && left.audioDevice == right.audioDevice && left.audioLatency == right.audioLatency &&
           left.input == right.input && left.showLibrary == right.showLibrary && left.overscan == right.overscan &&
           left.controllers == right.controllers && left.region == right.region;
}

class ConfigService {
public:
    [[nodiscard]] static Config defaults();
    [[nodiscard]] static Result<Config> fromJson(std::string_view jsonText);
    [[nodiscard]] static std::string toJson(const Config& config);
};

}  // namespace iris
