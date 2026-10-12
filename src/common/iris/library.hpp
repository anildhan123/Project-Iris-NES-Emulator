#pragma once

#include "iris/error.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace iris {

struct LibraryEntry {
    std::string path;
    std::string title;
    std::string detail;
    std::string contentId;
    std::int64_t addedUnixMs = 0;
    std::int64_t lastPlayedUnixMs = 0;

    bool operator==(const LibraryEntry&) const = default;
};

enum class LibrarySort {
    Title,
    LastPlayed,
};

struct GameLibrary {
    int version = 1;
    std::vector<LibraryEntry> games;

    bool operator==(const GameLibrary&) const = default;
};

class LibraryService {
public:
    static constexpr int kCurrentVersion = 1;
    static constexpr std::size_t kMaxGames = 2000;

    [[nodiscard]] static Result<GameLibrary> fromJson(std::string_view jsonText);
    [[nodiscard]] static std::string toJson(const GameLibrary& library);

    // Updates an existing path. A last-played time of zero leaves the stored time alone.
    static bool upsert(GameLibrary& library, LibraryEntry entry, std::string& error);
    static bool remove(GameLibrary& library, std::string_view path);
    [[nodiscard]] static std::vector<LibraryEntry> query(
        const GameLibrary& library, std::string_view text, LibrarySort sort);
    [[nodiscard]] static std::vector<LibraryEntry> recent(const GameLibrary& library, std::size_t limit);
};

}  // namespace iris
