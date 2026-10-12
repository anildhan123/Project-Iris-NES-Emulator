#pragma once

#include "iris/error.hpp"

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

struct Config {
    static constexpr int kCurrentVersion = 1;

    int version = kCurrentVersion;
    Theme theme = Theme::XpBlue;
    int windowWidth = 1024;
    int windowHeight = 768;
    LogLevel logLevel = LogLevel::Info;
};

[[nodiscard]] inline bool operator==(const Config& left, const Config& right) {
    return left.version == right.version && left.theme == right.theme &&
           left.windowWidth == right.windowWidth && left.windowHeight == right.windowHeight &&
           left.logLevel == right.logLevel;
}

class ConfigService {
public:
    [[nodiscard]] static Config defaults();
    [[nodiscard]] static Result<Config> fromJson(std::string_view jsonText);
    [[nodiscard]] static std::string toJson(const Config& config);
};

}  // namespace iris
