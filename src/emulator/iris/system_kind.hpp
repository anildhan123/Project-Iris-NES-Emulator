#pragma once

#include <cctype>
#include <filesystem>
#include <string>

namespace iris {

enum class SystemKind {
    Nes,
    Snes,
};

[[nodiscard]] inline const char* systemKindName(SystemKind kind) {
    return kind == SystemKind::Snes ? "Super NES" : "NES";
}

// A path with no Super NES extension stays on the NES, which is the original system.
[[nodiscard]] inline SystemKind systemKindForPath(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    for (char& character : extension) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    if (extension == ".sfc" || extension == ".smc") {
        return SystemKind::Snes;
    }
    return SystemKind::Nes;
}

}  // namespace iris
