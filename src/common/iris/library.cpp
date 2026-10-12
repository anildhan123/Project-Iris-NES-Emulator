#include "iris/library.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <utility>

namespace iris {
namespace {

using Json = nlohmann::ordered_json;

Result<GameLibrary> invalid(std::string message) {
    return Result<GameLibrary>::failure(ErrorCode::ValidationFailure, std::move(message));
}

bool hasOnly(const Json& object, std::initializer_list<const char*> keys, std::string& message) {
    if (!object.is_object()) {
        message = "Expected a JSON object.";
        return false;
    }
    for (const char* key : keys) {
        if (!object.contains(key)) {
            message = std::string("Missing library field: ") + key;
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
            message = "Unknown library field: " + it.key();
            return false;
        }
    }
    return true;
}

bool plainText(std::string_view text, std::size_t maximum, bool allowEmpty) {
    if (text.size() > maximum || (text.empty() && !allowEmpty)) {
        return false;
    }
    for (const unsigned char character : text) {
        if (character < 0x20 || character == 0x7F) {
            return false;
        }
    }
    return true;
}

bool contentIdText(std::string_view text) {
    if (text.empty()) {
        return true;
    }
    if (text.size() != 64) {
        return false;
    }
    for (const unsigned char character : text) {
        if (std::isxdigit(character) == 0) {
            return false;
        }
    }
    return true;
}

std::string folded(std::string_view text) {
    std::string out(text);
    for (char& character : out) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return out;
}

bool containsFolded(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) {
        return true;
    }
    return folded(haystack).find(folded(needle)) != std::string::npos;
}

}  // namespace

Result<GameLibrary> LibraryService::fromJson(std::string_view jsonText) {
    Json document;
    try {
        document = Json::parse(jsonText);
    } catch (const Json::exception& exception) {
        return Result<GameLibrary>::failure(ErrorCode::ParseFailure, exception.what());
    }
    if (!document.is_object()) {
        return invalid("Expected a JSON object.");
    }
    std::string message;
    if (!hasOnly(document, {"version", "games"}, message)) {
        return invalid(std::move(message));
    }
    if (!document["version"].is_number_integer() || document["version"].get<std::int64_t>() != kCurrentVersion) {
        return invalid("Unsupported library version. Expected version 1.");
    }
    if (!document["games"].is_array()) {
        return invalid("Library games must be a list.");
    }
    if (document["games"].size() > kMaxGames) {
        return invalid("The library has too many games.");
    }

    GameLibrary library;
    library.version = kCurrentVersion;
    try {
        for (const Json& item : document["games"]) {
            if (!hasOnly(item, {"path", "title", "detail", "contentId", "addedUnixMs", "lastPlayedUnixMs"}, message)) {
                return invalid(std::move(message));
            }
            if (!item["path"].is_string() || !item["title"].is_string() || !item["detail"].is_string() ||
                !item["contentId"].is_string()) {
                return invalid("Library text fields must be strings.");
            }
            LibraryEntry entry;
            entry.path = item["path"].get<std::string>();
            entry.title = item["title"].get<std::string>();
            entry.detail = item["detail"].get<std::string>();
            entry.contentId = item["contentId"].get<std::string>();
            if (!plainText(entry.path, 1024, false) || !plainText(entry.title, 256, true) ||
                !plainText(entry.detail, 256, true) || !contentIdText(entry.contentId)) {
                return invalid("A library entry has a field that is empty, too long, or not valid.");
            }
            if (!item["addedUnixMs"].is_number_integer() || !item["lastPlayedUnixMs"].is_number_integer()) {
                return invalid("Library times must be integers.");
            }
            entry.addedUnixMs = item["addedUnixMs"].get<std::int64_t>();
            entry.lastPlayedUnixMs = item["lastPlayedUnixMs"].get<std::int64_t>();
            if (entry.addedUnixMs < 0 || entry.lastPlayedUnixMs < 0) {
                return invalid("Library times must be zero or later.");
            }
            for (const LibraryEntry& existing : library.games) {
                if (existing.path == entry.path) {
                    return invalid("The library lists the same game twice.");
                }
            }
            library.games.push_back(std::move(entry));
        }
    } catch (const Json::exception&) {
        return invalid("Library values could not be read.");
    }
    return Result<GameLibrary>::success(std::move(library));
}

std::string LibraryService::toJson(const GameLibrary& library) {
    Json games = Json::array();
    for (const LibraryEntry& entry : library.games) {
        Json item = Json::object();
        item["path"] = entry.path;
        item["title"] = entry.title;
        item["detail"] = entry.detail;
        item["contentId"] = entry.contentId;
        item["addedUnixMs"] = entry.addedUnixMs;
        item["lastPlayedUnixMs"] = entry.lastPlayedUnixMs;
        games.push_back(std::move(item));
    }
    Json document = Json::object();
    document["version"] = kCurrentVersion;
    document["games"] = std::move(games);
    return document.dump(2);
}

bool LibraryService::upsert(GameLibrary& library, LibraryEntry entry, std::string& error) {
    if (!plainText(entry.path, 1024, false) || !plainText(entry.title, 256, true) || !plainText(entry.detail, 256, true) ||
        !contentIdText(entry.contentId) || entry.addedUnixMs < 0 || entry.lastPlayedUnixMs < 0) {
        error = "That game could not be added to the library.";
        return false;
    }
    for (LibraryEntry& game : library.games) {
        if (game.path == entry.path) {
            if (game.addedUnixMs > 0) {
                entry.addedUnixMs = game.addedUnixMs;
            }
            if (entry.lastPlayedUnixMs == 0) {
                entry.lastPlayedUnixMs = game.lastPlayedUnixMs;
            }
            game = std::move(entry);
            return true;
        }
    }
    if (library.games.size() >= kMaxGames) {
        error = "The library is full.";
        return false;
    }
    library.games.push_back(std::move(entry));
    return true;
}

bool LibraryService::remove(GameLibrary& library, std::string_view path) {
    const auto found = std::find_if(library.games.begin(), library.games.end(), [&](const LibraryEntry& game) {
        return game.path == path;
    });
    if (found == library.games.end()) {
        return false;
    }
    library.games.erase(found);
    return true;
}

std::vector<LibraryEntry> LibraryService::query(const GameLibrary& library, std::string_view text, LibrarySort sort) {
    std::vector<LibraryEntry> matched;
    for (const LibraryEntry& game : library.games) {
        if (containsFolded(game.title, text) || containsFolded(game.path, text)) {
            matched.push_back(game);
        }
    }
    if (sort == LibrarySort::LastPlayed) {
        std::stable_sort(matched.begin(), matched.end(), [](const LibraryEntry& left, const LibraryEntry& right) {
            if (left.lastPlayedUnixMs != right.lastPlayedUnixMs) {
                return left.lastPlayedUnixMs > right.lastPlayedUnixMs;
            }
            return folded(left.title) < folded(right.title);
        });
    } else {
        std::stable_sort(matched.begin(), matched.end(), [](const LibraryEntry& left, const LibraryEntry& right) {
            const std::string leftTitle = folded(left.title);
            const std::string rightTitle = folded(right.title);
            if (leftTitle != rightTitle) {
                return leftTitle < rightTitle;
            }
            return left.path < right.path;
        });
    }
    return matched;
}

std::vector<LibraryEntry> LibraryService::recent(const GameLibrary& library, std::size_t limit) {
    std::vector<LibraryEntry> played;
    for (const LibraryEntry& game : library.games) {
        if (game.lastPlayedUnixMs > 0) {
            played.push_back(game);
        }
    }
    std::stable_sort(played.begin(), played.end(), [](const LibraryEntry& left, const LibraryEntry& right) {
        return left.lastPlayedUnixMs > right.lastPlayedUnixMs;
    });
    if (played.size() > limit) {
        played.resize(limit);
    }
    return played;
}

}  // namespace iris
