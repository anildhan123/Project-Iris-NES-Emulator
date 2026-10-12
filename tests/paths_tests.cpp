#include "iris/paths.hpp"

#include "test_support.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("IRIS_DATA_DIR selects the data root and directories can be created") {
    iris::test::TemporaryDirectory directory;
    const iris::test::ScopedEnvironment environment("IRIS_DATA_DIR", directory.path().string());

    const auto paths = iris::resolveAppPaths();
    REQUIRE(paths.has_value());
    REQUIRE(paths->root == directory.path().lexically_normal());
    REQUIRE(paths->configFile == paths->configDirectory / "settings.json");
    REQUIRE(paths->logs == paths->root / "logs");
    REQUIRE(paths->states == paths->root / "states");
    REQUIRE(paths->saves == paths->root / "saves");
    REQUIRE(paths->screenshots == paths->root / "screenshots");
    REQUIRE(paths->shaders == paths->root / "shaders");
    REQUIRE(paths->library == paths->root / "library");

    REQUIRE(iris::ensureAppDirectories(*paths));
    REQUIRE(std::filesystem::is_directory(paths->configDirectory));
    REQUIRE(std::filesystem::is_directory(paths->logs));
    REQUIRE(std::filesystem::is_directory(paths->saves));
    REQUIRE(iris::ensureAppDirectories(*paths));
}

TEST_CASE("joinRelative rejects absolute paths and parent traversal") {
    const std::filesystem::path root = std::filesystem::path("C:/IrisData");

    const auto joined = iris::joinRelative(root, "library/game.json");
    REQUIRE(joined.has_value());
    REQUIRE(joined->filename() == "game.json");

    REQUIRE_FALSE(iris::joinRelative(root, "").has_value());
    REQUIRE_FALSE(iris::joinRelative(root, "..").has_value());
    REQUIRE_FALSE(iris::joinRelative(root, "../secret").has_value());
    REQUIRE_FALSE(iris::joinRelative(root, "library/../../outside").has_value());
    REQUIRE_FALSE(iris::joinRelative(root, "C:/Windows").has_value());
    REQUIRE_FALSE(iris::joinRelative(root, "/absolute").has_value());
}
