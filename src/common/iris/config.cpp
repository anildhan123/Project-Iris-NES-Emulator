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
    case Theme::XpBlue:
        return "xp-blue";
    case Theme::XpOlive:
        return "xp-olive";
    case Theme::XpSilver:
        return "xp-silver";
    }
    return "xp-blue";
}

std::optional<Theme> themeFromName(std::string_view name) {
    if (name == "xp-blue") {
        return Theme::XpBlue;
    }
    if (name == "xp-olive") {
        return Theme::XpOlive;
    }
    if (name == "xp-silver") {
        return Theme::XpSilver;
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

    std::string message;
    if (!hasOnly(document, {"version", "ui", "logging"}, message)) {
        return invalid(std::move(message));
    }

    try {
        if (!document["version"].is_number_integer() || document["version"].get<std::int64_t>() != Config::kCurrentVersion) {
            return invalid("Unsupported configuration version. Expected version 1.");
        }

        const Json& ui = document["ui"];
        if (!hasOnly(ui, {"theme", "windowWidth", "windowHeight"}, message)) {
            return invalid(std::move(message));
        }
        if (!ui["theme"].is_string()) {
            return invalid("ui.theme must be a string.");
        }
        const auto theme = themeFromName(ui["theme"].get<std::string>());
        if (!theme.has_value()) {
            return invalid("ui.theme must be xp-blue, xp-olive, or xp-silver.");
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

    Json document = Json::object();
    document["version"] = config.version;
    document["ui"] = std::move(ui);
    document["logging"] = std::move(logging);
    return document.dump(2);
}

}  // namespace iris
