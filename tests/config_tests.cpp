#include "iris/config.hpp"

#include "test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <string>

namespace {

std::string readFile(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

}  // namespace

TEST_CASE("configuration defaults round-trip through JSON") {
    const iris::Config defaults = iris::ConfigService::defaults();
    const auto parsed = iris::ConfigService::fromJson(iris::ConfigService::toJson(defaults));
    REQUIRE(parsed.ok());
    REQUIRE(parsed.value() == defaults);
    REQUIRE(parsed.value().theme == iris::Theme::Blue);
    REQUIRE(iris::ConfigService::toJson(defaults).find("\"blue\"") != std::string::npos);
    REQUIRE(parsed.value().windowWidth == 1024);
    REQUIRE(parsed.value().windowHeight == 768);
    REQUIRE(parsed.value().logLevel == iris::LogLevel::Info);
}

TEST_CASE("configuration accepts each theme and log level") {
    const char* json =
        R"({
  "version": 1,
  "ui": {
    "theme": "olive",
    "windowWidth": 7680,
    "windowHeight": 4320
  },
  "logging": {
    "level": "critical"
  }
})";
    const auto parsed = iris::ConfigService::fromJson(json);
    REQUIRE(parsed.ok());
    REQUIRE(parsed.value().theme == iris::Theme::Olive);
    REQUIRE(parsed.value().windowWidth == 7680);
    REQUIRE(parsed.value().windowHeight == 4320);
    REQUIRE(parsed.value().logLevel == iris::LogLevel::Critical);

    const auto saved = iris::ConfigService::fromJson(iris::ConfigService::toJson(parsed.value()));
    REQUIRE(saved.ok());
    REQUIRE(saved.value() == parsed.value());
}

TEST_CASE("configuration still reads the previous theme ids") {
    const auto parsed = iris::ConfigService::fromJson(
        R"({"version":1,"ui":{"theme":"xp-silver","windowWidth":1024,"windowHeight":768},"logging":{"level":"info"}})");
    REQUIRE(parsed.ok());
    REQUIRE(parsed.value().theme == iris::Theme::Silver);
    const std::string saved = iris::ConfigService::toJson(parsed.value());
    REQUIRE(saved.find("\"silver\"") != std::string::npos);
    REQUIRE(saved.find("xp-") == std::string::npos);
}

TEST_CASE("configuration rejects malformed JSON, unknown fields, themes, and sizes") {
    const auto malformed = iris::ConfigService::fromJson("{");
    REQUIRE_FALSE(malformed.ok());
    REQUIRE(malformed.error().code == iris::ErrorCode::ParseFailure);

    const auto unknownTheme = iris::ConfigService::fromJson(
        R"({"version":1,"ui":{"theme":"neon","windowWidth":1024,"windowHeight":768},"logging":{"level":"info"}})");
    REQUIRE_FALSE(unknownTheme.ok());
    REQUIRE(unknownTheme.error().code == iris::ErrorCode::ValidationFailure);

    const auto tinyWindow = iris::ConfigService::fromJson(
        R"({"version":1,"ui":{"theme":"xp-blue","windowWidth":100,"windowHeight":768},"logging":{"level":"info"}})");
    REQUIRE_FALSE(tinyWindow.ok());
    REQUIRE(tinyWindow.error().code == iris::ErrorCode::ValidationFailure);

    const auto shortWindow = iris::ConfigService::fromJson(
        R"({"version":1,"ui":{"theme":"xp-silver","windowWidth":800,"windowHeight":479},"logging":{"level":"info"}})");
    REQUIRE_FALSE(shortWindow.ok());

    const auto extraField = iris::ConfigService::fromJson(
        R"({"version":1,"ui":{"theme":"xp-blue","windowWidth":1024,"windowHeight":768},"logging":{"level":"info"},"video":{}})");
    REQUIRE_FALSE(extraField.ok());
    REQUIRE(extraField.error().message.find("video") != std::string::npos);

    const auto futureVersion = iris::ConfigService::fromJson(
        R"({"version":2,"ui":{"theme":"xp-blue","windowWidth":1024,"windowHeight":768},"logging":{"level":"info"}})");
    REQUIRE_FALSE(futureVersion.ok());
    REQUIRE(futureVersion.error().code == iris::ErrorCode::ValidationFailure);
}

TEST_CASE("configuration files save atomically and malformed files are left unchanged") {
    iris::test::TemporaryDirectory directory;
    const auto file = directory.path() / "settings.json";

    const auto missing = iris::loadConfigFile(file);
    REQUIRE_FALSE(missing.ok());
    REQUIRE(missing.error().code == iris::ErrorCode::NotFound);

    iris::Config config = iris::ConfigService::defaults();
    config.theme = iris::Theme::Silver;
    config.windowWidth = 1280;
    config.windowHeight = 720;
    config.logLevel = iris::LogLevel::Warn;
    REQUIRE(iris::saveConfigFile(file, config));
    std::filesystem::path temporary = file;
    temporary += ".tmp";
    REQUIRE_FALSE(std::filesystem::exists(temporary));

    const auto loaded = iris::loadConfigFile(file);
    REQUIRE(loaded.ok());
    REQUIRE(loaded.value() == config);

    const std::string broken = "{";
    {
        std::ofstream output(file, std::ios::binary | std::ios::trunc);
        output << broken;
    }
    const auto rejected = iris::loadConfigFile(file);
    REQUIRE_FALSE(rejected.ok());
    REQUIRE(rejected.error().code == iris::ErrorCode::ParseFailure);
    REQUIRE(readFile(file) == broken);

    const std::string withBom = std::string("\xEF\xBB\xBF") + iris::ConfigService::toJson(config);
    {
        std::ofstream output(file, std::ios::binary | std::ios::trunc);
        output << withBom;
    }
    const auto bomLoaded = iris::loadConfigFile(file);
    REQUIRE(bomLoaded.ok());
    REQUIRE(bomLoaded.value() == config);
}
