#pragma once

#include "iris/error.hpp"
#include "iris/input_bindings.hpp"
#include "iris/presentation.hpp"

#include <string>
#include <string_view>

namespace iris {

inline constexpr int kMinWindowWidth = 640;
inline constexpr int kMinWindowHeight = 480;
inline constexpr int kMaxWindowWidth = 7680;
inline constexpr int kMaxWindowHeight = 4320;

enum class Theme {
    XpBlue,
    XpOlive,
    XpSilver,
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

struct Config {
    static constexpr int kCurrentVersion = 3;

    int version = kCurrentVersion;
    Theme theme = Theme::XpBlue;
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
           left.input == right.input && left.showLibrary == right.showLibrary;
}

class ConfigService {
public:
    [[nodiscard]] static Config defaults();
    [[nodiscard]] static Result<Config> fromJson(std::string_view jsonText);
    [[nodiscard]] static std::string toJson(const Config& config);
};

}  // namespace iris
