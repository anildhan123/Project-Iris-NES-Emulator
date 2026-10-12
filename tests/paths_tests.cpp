#include "iris/paths.hpp"

#include "test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <set>
#include <string>

namespace {

void touch(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << "x";
}

std::set<std::string> filenames(const iris::RomScan& scan) {
    std::set<std::string> names;
    for (const std::filesystem::path& file : scan.files) {
        names.insert(file.filename().string());
    }
    return names;
}

}  // namespace

TEST_CASE("a folder scan includes subfolders and skips other files") {
    iris::test::TemporaryDirectory directory;
    const auto root = directory.path();
    touch(root / "top.nes");
    touch(root / "notes.txt");
    touch(root / "one" / "mid.NES");
    touch(root / "one" / "two" / "deep.nes");

    const iris::RomScan scan = iris::collectNesFiles(root);
    const auto names = filenames(scan);
    REQUIRE(names.size() == 3);
    REQUIRE(names.count("top.nes") == 1);
    REQUIRE(names.count("mid.NES") == 1);
    REQUIRE(names.count("deep.nes") == 1);
    REQUIRE_FALSE(scan.capped);
    REQUIRE_FALSE(scan.depthLimited);
    REQUIRE_FALSE(scan.incomplete);

    const iris::RomScan topOnly = iris::collectNesFiles(root, 0);
    REQUIRE(filenames(topOnly) == std::set<std::string>{"top.nes"});
    REQUIRE(topOnly.depthLimited);

    const iris::RomScan oneLevel = iris::collectNesFiles(root, 1);
    const auto oneLevelNames = filenames(oneLevel);
    REQUIRE(oneLevelNames.count("top.nes") == 1);
    REQUIRE(oneLevelNames.count("mid.NES") == 1);
    REQUIRE(oneLevelNames.count("deep.nes") == 0);
    REQUIRE(oneLevel.depthLimited);

    const iris::RomScan capped = iris::collectNesFiles(root, 16, 1);
    REQUIRE(capped.files.size() == 1);
    REQUIRE(capped.capped);

    const iris::RomScan missing = iris::collectNesFiles(root / "absent");
    REQUIRE(missing.files.empty());
    REQUIRE(missing.incomplete);
}

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

TEST_CASE("Windows and XDG data roots follow the platform variables") {
    const iris::DataEnvironment empty{};
    REQUIRE_FALSE(iris::defaultDataRoot(iris::DataLayout::Windows, empty).has_value());
    REQUIRE_FALSE(iris::defaultDataRoot(iris::DataLayout::Xdg, empty).has_value());

    iris::DataEnvironment local{};
    local.localAppData = "C:/Users/me/AppData/Local";
    const auto fromLocal = iris::defaultDataRoot(iris::DataLayout::Windows, local);
    REQUIRE(fromLocal.has_value());
    REQUIRE(fromLocal->filename() == "IrisEmulator");

    iris::DataEnvironment profile{};
    profile.userProfile = "C:/Users/me";
    const auto fromProfile = iris::defaultDataRoot(iris::DataLayout::Windows, profile);
    REQUIRE(fromProfile.has_value());
    REQUIRE(fromProfile->generic_string().find("AppData/Local/IrisEmulator") != std::string::npos);

    iris::DataEnvironment xdg{};
    xdg.xdgDataHome = "/home/me/.local/share";
    const auto fromXdg = iris::defaultDataRoot(iris::DataLayout::Xdg, xdg);
    REQUIRE(fromXdg.has_value());
    REQUIRE(fromXdg->filename() == "IrisEmulator");

    iris::DataEnvironment home{};
    home.home = "/home/me";
    const auto fromHome = iris::defaultDataRoot(iris::DataLayout::Xdg, home);
    REQUIRE(fromHome.has_value());
    REQUIRE(fromHome->generic_string().find(".local/share/IrisEmulator") != std::string::npos);

    iris::DataEnvironment overridden{};
    overridden.irisDataDir = "D:/IrisData";
    overridden.home = "/home/me";
    overridden.localAppData = "C:/Users/me/AppData/Local";
    const auto fromOverride = iris::defaultDataRoot(iris::DataLayout::Xdg, overridden);
    REQUIRE(fromOverride.has_value());
    REQUIRE(fromOverride->filename() == "IrisData");
}
