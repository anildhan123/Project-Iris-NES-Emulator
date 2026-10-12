#include "iris/paths.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace iris {
namespace {

constexpr std::uintmax_t kMaxConfigBytes = 1024 * 1024;
constexpr std::uintmax_t kMaxLibraryBytes = 1024 * 1024;

AppPaths pathsFromRoot(std::filesystem::path root) {
    AppPaths paths;
    paths.root = std::move(root);
    paths.configDirectory = paths.root / "config";
    paths.configFile = paths.configDirectory / "settings.json";
    paths.logs = paths.root / "logs";
    paths.states = paths.root / "states";
    paths.saves = paths.root / "saves";
    paths.screenshots = paths.root / "screenshots";
    paths.shaders = paths.root / "shaders";
    paths.library = paths.root / "library";
    return paths;
}

}  // namespace

std::optional<std::string> environmentValue(const char* name) {
#if defined(_WIN32)
    char* buffer = nullptr;
    std::size_t length = 0;
    if (_dupenv_s(&buffer, &length, name) != 0 || buffer == nullptr || buffer[0] == '\0') {
        std::free(buffer);
        return std::nullopt;
    }
    std::string value(buffer);
    std::free(buffer);
    return value;
#else
    const char* value = std::getenv(name);
    if (value == nullptr || value[0] == '\0') {
        return std::nullopt;
    }
    return std::string(value);
#endif
}

std::optional<AppPaths> resolveAppPaths() {
    if (const auto overridePath = environmentValue("IRIS_DATA_DIR")) {
        return pathsFromRoot(std::filesystem::path(*overridePath).lexically_normal());
    }
    if (const auto localAppData = environmentValue("LOCALAPPDATA")) {
        return pathsFromRoot((std::filesystem::path(*localAppData) / "IrisEmulator").lexically_normal());
    }
    if (const auto userProfile = environmentValue("USERPROFILE")) {
        return pathsFromRoot(
            (std::filesystem::path(*userProfile) / "AppData" / "Local" / "IrisEmulator").lexically_normal());
    }
    return std::nullopt;
}

bool ensureAppDirectories(const AppPaths& paths) {
    const std::filesystem::path directories[] = {
        paths.root,
        paths.configDirectory,
        paths.logs,
        paths.states,
        paths.saves,
        paths.screenshots,
        paths.shaders,
        paths.library,
    };
    for (const auto& directory : directories) {
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        if (error) {
            return false;
        }
    }
    return true;
}

std::optional<std::filesystem::path> joinRelative(
    const std::filesystem::path& root,
    const std::filesystem::path& relative) {
    if (relative.empty() || relative.is_absolute() || relative.has_root_name() || relative.has_root_directory()) {
        return std::nullopt;
    }
    for (const auto& part : relative) {
        if (part == "..") {
            return std::nullopt;
        }
    }

    const auto normalRoot = root.lexically_normal();
    const auto combined = (normalRoot / relative).lexically_normal();
    const auto remainder = combined.lexically_relative(normalRoot);
    if (remainder.empty() || *remainder.begin() == "..") {
        return std::nullopt;
    }
    return combined;
}

bool writeBinaryFileAtomically(const std::filesystem::path& destination, const std::uint8_t* data, std::size_t size) {
    const auto temporary = destination.native() +
#if defined(_WIN32)
                           L".tmp";
#else
                           ".tmp";
#endif
    const std::filesystem::path temporaryPath(temporary);
    {
        std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!output) {
            return false;
        }
        if (data != nullptr && size > 0) {
            output.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        }
        output.flush();
        if (!output) {
            output.close();
            std::filesystem::remove(temporaryPath);
            return false;
        }
    }

    std::error_code error;
#if defined(_WIN32)
    if (std::filesystem::exists(destination, error) && !error) {
        if (ReplaceFileW(
                destination.c_str(),
                temporaryPath.c_str(),
                nullptr,
                REPLACEFILE_IGNORE_MERGE_ERRORS,
                nullptr,
                nullptr) != 0) {
            return true;
        }
        std::filesystem::remove(temporaryPath);
        return false;
    }
#endif
    std::filesystem::rename(temporaryPath, destination, error);
    if (error) {
        std::filesystem::remove(temporaryPath);
        return false;
    }
    return true;
}

bool writeTextFileAtomically(const std::filesystem::path& destination, std::string_view contents) {
    return writeBinaryFileAtomically(
        destination, reinterpret_cast<const std::uint8_t*>(contents.data()), contents.size());
}

Result<Config> loadConfigFile(const std::filesystem::path& path) {
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) {
        return Result<Config>::failure(ErrorCode::IoFailure, error.message());
    }
    if (!exists) {
        return Result<Config>::failure(ErrorCode::NotFound, "Configuration file does not exist.");
    }
    if (!std::filesystem::is_regular_file(path, error) || error) {
        return Result<Config>::failure(ErrorCode::IoFailure, "Configuration path is not a file.");
    }

    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        return Result<Config>::failure(ErrorCode::IoFailure, error.message());
    }
    if (size > kMaxConfigBytes) {
        return Result<Config>::failure(ErrorCode::ValidationFailure, "Configuration file exceeds the size limit.");
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<Config>::failure(ErrorCode::IoFailure, "Could not open the configuration file.");
    }
    std::string text(static_cast<std::size_t>(size), '\0');
    input.read(text.data(), static_cast<std::streamsize>(size));
    if (!input) {
        return Result<Config>::failure(ErrorCode::IoFailure, "Could not read the configuration file.");
    }
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
    return ConfigService::fromJson(text);
}

bool saveConfigFile(const std::filesystem::path& path, const Config& config) {
    return writeTextFileAtomically(path, ConfigService::toJson(config) + "\n");
}

Result<GameLibrary> loadLibraryFile(const std::filesystem::path& path) {
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error) {
        return Result<GameLibrary>::failure(ErrorCode::IoFailure, error.message());
    }
    if (!exists) {
        return Result<GameLibrary>::failure(ErrorCode::NotFound, "Library file does not exist.");
    }
    if (!std::filesystem::is_regular_file(path, error) || error) {
        return Result<GameLibrary>::failure(ErrorCode::IoFailure, "Library path is not a file.");
    }
    const auto size = std::filesystem::file_size(path, error);
    if (error) {
        return Result<GameLibrary>::failure(ErrorCode::IoFailure, error.message());
    }
    if (size > kMaxLibraryBytes) {
        return Result<GameLibrary>::failure(ErrorCode::ValidationFailure, "Library file exceeds the size limit.");
    }
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return Result<GameLibrary>::failure(ErrorCode::IoFailure, "Could not open the library file.");
    }
    std::string text(static_cast<std::size_t>(size), '\0');
    input.read(text.data(), static_cast<std::streamsize>(size));
    if (!input) {
        return Result<GameLibrary>::failure(ErrorCode::IoFailure, "Could not read the library file.");
    }
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
    return LibraryService::fromJson(text);
}

bool saveLibraryFile(const std::filesystem::path& path, const GameLibrary& library) {
    return writeTextFileAtomically(path, LibraryService::toJson(library) + "\n");
}

RomScan collectNesFiles(const std::filesystem::path& folder, int maxDepth, std::size_t maxFiles) {
    RomScan scan;
    if (maxDepth < 0) {
        maxDepth = 0;
    }
    std::error_code failure;
    if (!std::filesystem::is_directory(folder, failure) || failure) {
        scan.incomplete = true;
        return scan;
    }
    std::filesystem::recursive_directory_iterator cursor(
        folder, std::filesystem::directory_options::skip_permission_denied, failure);
    const std::filesystem::recursive_directory_iterator end;
    for (; !failure && cursor != end; cursor.increment(failure)) {
        std::error_code entryError;
        if (cursor->is_directory(entryError)) {
            if (cursor.depth() >= maxDepth) {
                cursor.disable_recursion_pending();
                scan.depthLimited = true;
            }
            continue;
        }
        if (entryError || !cursor->is_regular_file(entryError)) {
            continue;
        }
        std::string extension = cursor->path().extension().string();
        for (char& character : extension) {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }
        if (extension != ".nes") {
            continue;
        }
        if (scan.files.size() >= maxFiles) {
            scan.capped = true;
            break;
        }
        scan.files.push_back(cursor->path());
    }
    if (failure) {
        scan.incomplete = true;
    }
    return scan;
}

}  // namespace iris
