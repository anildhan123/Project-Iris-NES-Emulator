#pragma once

#include "iris/config.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <cstdint>

namespace iris {

struct AppPaths {
    std::filesystem::path root;
    std::filesystem::path configDirectory;
    std::filesystem::path configFile;
    std::filesystem::path logs;
    std::filesystem::path states;
    std::filesystem::path saves;
    std::filesystem::path screenshots;
    std::filesystem::path shaders;
    std::filesystem::path library;
};

[[nodiscard]] std::optional<std::string> environmentValue(const char* name);

// IRIS_DATA_DIR when set, otherwise %LOCALAPPDATA%\IrisEmulator.
[[nodiscard]] std::optional<AppPaths> resolveAppPaths();

[[nodiscard]] bool ensureAppDirectories(const AppPaths& paths);

// Joins a relative path that must stay inside root. Rejects absolute paths and "..".
[[nodiscard]] std::optional<std::filesystem::path> joinRelative(
    const std::filesystem::path& root,
    const std::filesystem::path& relative);

[[nodiscard]] bool writeTextFileAtomically(
    const std::filesystem::path& destination,
    std::string_view contents);

// Writes a temporary file beside destination, then replaces destination.
[[nodiscard]] bool writeBinaryFileAtomically(
    const std::filesystem::path& destination,
    const std::uint8_t* data,
    std::size_t size);

[[nodiscard]] Result<Config> loadConfigFile(const std::filesystem::path& path);
[[nodiscard]] bool saveConfigFile(const std::filesystem::path& path, const Config& config);

}  // namespace iris
