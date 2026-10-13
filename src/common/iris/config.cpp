#include "iris/config.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>

namespace iris {
namespace {

using Json = nlohmann::ordered_json;

const char* themeName(Theme theme) {
    switch (theme) {
    case Theme::Blue:
        return "blue";
    case Theme::Olive:
        return "olive";
    case Theme::Silver:
        return "silver";
    }
    return "blue";
}

std::optional<Theme> themeFromName(std::string_view name) {
    if (name == "blue" || name == "xp-blue") {
        return Theme::Blue;
    }
    if (name == "olive" || name == "xp-olive") {
        return Theme::Olive;
    }
    if (name == "silver" || name == "xp-silver") {
        return Theme::Silver;
    }
    return std::nullopt;
}

const char* levelName(LogLevel level) {
    switch (level) {
    case LogLevel::Trace:
        return "trace";
    case LogLevel::Debug:
        return "debug";
    case LogLevel::Info:
        return "info";
    case LogLevel::Warn:
        return "warn";
    case LogLevel::Error:
        return "error";
    case LogLevel::Critical:
        return "critical";
    }
    return "info";
}

const char* latencyName(AudioLatency latency) {
    switch (latency) {
    case AudioLatency::Short:
        return "short";
    case AudioLatency::Balanced:
        return "balanced";
    case AudioLatency::Long:
        return "long";
    }
    return "balanced";
}

std::optional<AudioLatency> latencyFromName(std::string_view name) {
    if (name == "short") {
        return AudioLatency::Short;
    }
    if (name == "balanced") {
        return AudioLatency::Balanced;
    }
    if (name == "long") {
        return AudioLatency::Long;
    }
    return std::nullopt;
}

bool acceptableDevice(std::string_view name) {
    if (name.size() > 256) {
        return false;
    }
    for (const unsigned char character : name) {
        if (character < 0x20 || character == 0x7F) {
            return false;
        }
    }
    return true;
}

bool readScancode(const Json& object, const char* key, int& destination, std::string& message) {
    if (!object[key].is_number_integer()) {
        message = std::string(key) + " must be an integer key code.";
        return false;
    }
    const auto value = object[key].get<std::int64_t>();
    if (value < kMinScancode || value > kMaxScancode) {
        message = std::string(key) + " is outside the supported key range.";
        return false;
    }
    destination = static_cast<int>(value);
    return true;
}

std::optional<LogLevel> levelFromName(std::string_view name) {
    if (name == "trace") {
        return LogLevel::Trace;
    }
    if (name == "debug") {
        return LogLevel::Debug;
    }
    if (name == "info") {
        return LogLevel::Info;
    }
    if (name == "warn") {
        return LogLevel::Warn;
    }
    if (name == "error") {
        return LogLevel::Error;
    }
    if (name == "critical") {
        return LogLevel::Critical;
    }
    return std::nullopt;
}

Result<Config> invalid(std::string message) {
    return Result<Config>::failure(ErrorCode::ValidationFailure, std::move(message));
}

bool hasOnly(const Json& object, std::initializer_list<const char*> keys, std::string& message) {
    if (!object.is_object()) {
        message = "Expected a JSON object.";
        return false;
    }
    for (const char* key : keys) {
        if (!object.contains(key)) {
            message = std::string("Missing configuration field: ") + key;
            return false;
        }
    }
    for (auto it = object.begin(); it != object.end(); ++it) {
        bool known = false;
        for (const char* key : keys) {
            if (it.key() == key) {
                known = true;
                break;
            }
        }
        if (!known) {
            message = "Unknown configuration field: " + it.key();
            return false;
        }
    }
    return true;
}

}  // namespace

Config ConfigService::defaults() {
    return Config{};
}

Result<Config> ConfigService::fromJson(std::string_view jsonText) {
    Json document;
    try {
        document = Json::parse(jsonText);
    } catch (const Json::exception& exception) {
        return Result<Config>::failure(ErrorCode::ParseFailure, exception.what());
    }

    if (!document.is_object()) {
        return invalid("Expected a JSON object.");
    }
    if (!document.contains("version") || !document["version"].is_number_integer()) {
        return invalid("Unsupported configuration version. Expected version 1, 2, or 3.");
    }
    const auto version = document["version"].get<std::int64_t>();
    if (version != 1 && version != 2 && version != 3) {
        return invalid("Unsupported configuration version. Expected version 1, 2, or 3.");
    }

    std::string message;
    if (version == 1) {
        if (!hasOnly(document, {"version", "ui", "logging"}, message)) {
            return invalid(std::move(message));
        }
    } else if (version == 2) {
        if (!hasOnly(document, {"version", "ui", "logging", "video"}, message)) {
            return invalid(std::move(message));
        }
    } else if (!hasOnly(document, {"version", "ui", "logging", "video", "audio", "input", "library"}, message)) {
        return invalid(std::move(message));
    }

    try {

        const Json& ui = document["ui"];
        if (!hasOnly(ui, {"theme", "windowWidth", "windowHeight"}, message)) {
            return invalid(std::move(message));
        }
        if (!ui["theme"].is_string()) {
            return invalid("ui.theme must be a string.");
        }
        const auto theme = themeFromName(ui["theme"].get<std::string>());
        if (!theme.has_value()) {
            return invalid("ui.theme must be blue, olive, or silver.");
        }
        if (!ui["windowWidth"].is_number_integer() || !ui["windowHeight"].is_number_integer()) {
            return invalid("Window width and height must be integers.");
        }
        const auto width = ui["windowWidth"].get<std::int64_t>();
        const auto height = ui["windowHeight"].get<std::int64_t>();
        if (width < kMinWindowWidth || width > kMaxWindowWidth || height < kMinWindowHeight || height > kMaxWindowHeight) {
            return invalid("Window size must be between 640x480 and 7680x4320.");
        }

        const Json& logging = document["logging"];
        if (!hasOnly(logging, {"level"}, message)) {
            return invalid(std::move(message));
        }
        if (!logging["level"].is_string()) {
            return invalid("logging.level must be a string.");
        }
        const auto level = levelFromName(logging["level"].get<std::string>());
        if (!level.has_value()) {
            return invalid("logging.level must be trace, debug, info, warn, error, or critical.");
        }

        Config config;
        config.version = Config::kCurrentVersion;
        config.theme = *theme;
        config.windowWidth = static_cast<int>(width);
        config.windowHeight = static_cast<int>(height);
        config.logLevel = *level;
        if (version >= 2) {
            const Json& video = document["video"];
            if (!hasOnly(
                    video,
                    {"scaleMode", "integerScale", "aspectRatio", "filter", "fullscreen", "vsync", "showPerformance",
                     "brightness", "contrast", "saturation", "gamma", "sharpness", "scanline"},
                    message)) {
                return invalid(std::move(message));
            }
            if (!video["scaleMode"].is_string() || !video["aspectRatio"].is_string() || !video["filter"].is_string()) {
                return invalid("Video mode names must be strings.");
            }
            const auto scaleMode = scaleModeFromId(video["scaleMode"].get<std::string>());
            if (!scaleMode.has_value()) {
                return invalid("video.scaleMode must be fit or integer.");
            }
            const auto aspect = aspectModeFromId(video["aspectRatio"].get<std::string>());
            if (!aspect.has_value()) {
                return invalid("video.aspectRatio must be pixel or 4:3.");
            }
            const auto filter = pictureFilterFromId(video["filter"].get<std::string>());
            if (!filter.has_value()) {
                return invalid("video.filter must be nearest, smooth, sharp, or crt.");
            }
            if (!video["integerScale"].is_number_integer()) {
                return invalid("video.integerScale must be an integer from 1 to 6.");
            }
            const auto integerScale = video["integerScale"].get<std::int64_t>();
            if (integerScale < kMinimumIntegerScale || integerScale > kMaximumIntegerScale) {
                return invalid("video.integerScale must be an integer from 1 to 6.");
            }
            if (!video["fullscreen"].is_boolean() || !video["vsync"].is_boolean() || !video["showPerformance"].is_boolean()) {
                return invalid("Fullscreen, VSync, and performance must be true or false.");
            }
            const auto readGrade = [&](const char* key, const ParameterRange& parameter, float& destination) {
                if (!video[key].is_number() || !acceptsParameter(parameter, video[key].get<double>())) {
                    return false;
                }
                destination = clampParameter(parameter, video[key].get<double>());
                return true;
            };
            if (!readGrade("brightness", kBrightness, config.brightness) ||
                !readGrade("contrast", kContrast, config.contrast) ||
                !readGrade("saturation", kSaturation, config.saturation) ||
                !readGrade("gamma", kGamma, config.gamma) || !readGrade("sharpness", kSharpness, config.sharpness) ||
                !readGrade("scanline", kScanline, config.scanline)) {
                return invalid("A picture setting is outside its allowed range.");
            }
            config.scaleMode = *scaleMode;
            config.integerScale = static_cast<int>(integerScale);
            config.aspect = *aspect;
            config.filter = *filter;
            config.fullscreen = video["fullscreen"].get<bool>();
            config.vsync = video["vsync"].get<bool>();
            config.showPerformance = video["showPerformance"].get<bool>();
        }
        if (version == 3) {
            const Json& audio = document["audio"];
            if (!hasOnly(audio, {"volume", "muted", "device", "latency"}, message)) {
                return invalid(std::move(message));
            }
            if (!audio["volume"].is_number()) {
                return invalid("audio.volume must be a number from 0 to 1.");
            }
            const double volume = audio["volume"].get<double>();
            if (volume < 0.0 || volume > 1.0) {
                return invalid("audio.volume must be a number from 0 to 1.");
            }
            if (!audio["muted"].is_boolean()) {
                return invalid("audio.muted must be true or false.");
            }
            if (!audio["device"].is_string()) {
                return invalid("audio.device must be a string.");
            }
            const std::string device = audio["device"].get<std::string>();
            if (!acceptableDevice(device)) {
                return invalid("audio.device is too long or contains a control character.");
            }
            if (!audio["latency"].is_string()) {
                return invalid("audio.latency must be short, balanced, or long.");
            }
            const auto latency = latencyFromName(audio["latency"].get<std::string>());
            if (!latency.has_value()) {
                return invalid("audio.latency must be short, balanced, or long.");
            }

            const Json& input = document["input"];
            if (!hasOnly(
                    input,
                    {"a", "b", "select", "start", "up", "down", "left", "right", "openGame", "quickSave", "quickLoad",
                     "pause", "fullscreen", "reset"},
                    message)) {
                return invalid(std::move(message));
            }
            InputBindings bindings;
            if (!readScancode(input, "a", bindings.a, message) || !readScancode(input, "b", bindings.b, message) ||
                !readScancode(input, "select", bindings.select, message) ||
                !readScancode(input, "start", bindings.start, message) || !readScancode(input, "up", bindings.up, message) ||
                !readScancode(input, "down", bindings.down, message) ||
                !readScancode(input, "left", bindings.left, message) ||
                !readScancode(input, "right", bindings.right, message) ||
                !readScancode(input, "openGame", bindings.openGame, message) ||
                !readScancode(input, "quickSave", bindings.quickSave, message) ||
                !readScancode(input, "quickLoad", bindings.quickLoad, message) ||
                !readScancode(input, "pause", bindings.pause, message) ||
                !readScancode(input, "fullscreen", bindings.fullscreen, message) ||
                !readScancode(input, "reset", bindings.reset, message)) {
                return invalid(std::move(message));
            }
            if (const std::optional<std::string> conflict = bindingConflict(bindings)) {
                return invalid(*conflict);
            }

            const Json& library = document["library"];
            if (!hasOnly(library, {"show"}, message)) {
                return invalid(std::move(message));
            }
            if (!library["show"].is_boolean()) {
                return invalid("library.show must be true or false.");
            }
            config.volume = static_cast<float>(volume);
            config.muted = audio["muted"].get<bool>();
            config.audioDevice = device;
            config.audioLatency = *latency;
            config.input = bindings;
            config.showLibrary = library["show"].get<bool>();
        }
        return Result<Config>::success(std::move(config));
    } catch (const Json::exception&) {
        return invalid("Configuration values could not be read.");
    }
}

std::string ConfigService::toJson(const Config& config) {
    Json ui = Json::object();
    ui["theme"] = themeName(config.theme);
    ui["windowWidth"] = config.windowWidth;
    ui["windowHeight"] = config.windowHeight;

    Json logging = Json::object();
    logging["level"] = levelName(config.logLevel);

    Json video = Json::object();
    video["scaleMode"] = scaleModeId(config.scaleMode);
    video["integerScale"] = config.integerScale;
    video["aspectRatio"] = aspectModeId(config.aspect);
    video["filter"] = pictureFilterId(config.filter);
    video["fullscreen"] = config.fullscreen;
    video["vsync"] = config.vsync;
    video["showPerformance"] = config.showPerformance;
    video["brightness"] = config.brightness;
    video["contrast"] = config.contrast;
    video["saturation"] = config.saturation;
    video["gamma"] = config.gamma;
    video["sharpness"] = config.sharpness;
    video["scanline"] = config.scanline;

    Json audio = Json::object();
    audio["volume"] = config.volume;
    audio["muted"] = config.muted;
    audio["device"] = config.audioDevice;
    audio["latency"] = latencyName(config.audioLatency);

    Json input = Json::object();
    input["a"] = config.input.a;
    input["b"] = config.input.b;
    input["select"] = config.input.select;
    input["start"] = config.input.start;
    input["up"] = config.input.up;
    input["down"] = config.input.down;
    input["left"] = config.input.left;
    input["right"] = config.input.right;
    input["openGame"] = config.input.openGame;
    input["quickSave"] = config.input.quickSave;
    input["quickLoad"] = config.input.quickLoad;
    input["pause"] = config.input.pause;
    input["fullscreen"] = config.input.fullscreen;
    input["reset"] = config.input.reset;

    Json library = Json::object();
    library["show"] = config.showLibrary;

    Json document = Json::object();
    document["version"] = Config::kCurrentVersion;
    document["ui"] = std::move(ui);
    document["logging"] = std::move(logging);
    document["video"] = std::move(video);
    document["audio"] = std::move(audio);
    document["input"] = std::move(input);
    document["library"] = std::move(library);
    return document.dump(2);
}

}  // namespace iris
