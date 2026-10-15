#include "iris/config.hpp"
#include "iris/emulated_system.hpp"
#include "iris/input_bindings.hpp"
#include "iris/library.hpp"
#include "iris/paths.hpp"

#include "test_support.hpp"

#include <catch2/catch_test_macros.hpp>

#include <fstream>

namespace {

iris::LibraryEntry game(std::string path, std::string title, std::int64_t played) {
    iris::LibraryEntry entry;
    entry.path = std::move(path);
    entry.title = std::move(title);
    entry.detail = "NROM, NTSC";
    entry.contentId = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    entry.addedUnixMs = 10;
    entry.lastPlayedUnixMs = played;
    return entry;
}

}  // namespace

TEST_CASE("library JSON round-trips and rejects unknown fields") {
    iris::GameLibrary library;
    library.games.push_back(game("C:/games/one.nes", "One", 20));
    library.games.push_back(game("C:/games/two.nes", "Two", 0));
    const auto parsed = iris::LibraryService::fromJson(iris::LibraryService::toJson(library));
    REQUIRE(parsed.ok());
    REQUIRE(parsed.value() == library);

    const auto missingFlag = iris::LibraryService::fromJson(
        R"({"version":1,"games":[{"path":"C:/games/one.nes","title":"One","detail":"NROM, NTSC","contentId":"","addedUnixMs":1,"lastPlayedUnixMs":0,"missing":true}]})");
    REQUIRE_FALSE(missingFlag.ok());
}

TEST_CASE("removing a library entry does not delete the game file") {
    iris::test::TemporaryDirectory directory;
    const auto file = directory.path() / "kept.nes";
    {
        std::ofstream output(file, std::ios::binary);
        output << "not deleted";
    }
    iris::GameLibrary library;
    std::string error;
    REQUIRE(iris::LibraryService::upsert(library, game(file.string(), "Kept", 5), error));
    REQUIRE(iris::LibraryService::remove(library, file.string()));
    REQUIRE(library.games.empty());
    REQUIRE(std::filesystem::is_regular_file(file));
}

TEST_CASE("library search, sort, and recent order") {
    iris::GameLibrary library;
    std::string error;
    REQUIRE(iris::LibraryService::upsert(library, game("a.nes", "Zelda", 10), error));
    REQUIRE(iris::LibraryService::upsert(library, game("b.nes", "Metroid", 30), error));
    REQUIRE(iris::LibraryService::upsert(library, game("c.nes", "Marble", 0), error));

    const auto found = iris::LibraryService::query(library, "met", iris::LibrarySort::Title);
    REQUIRE(found.size() == 1);
    REQUIRE(found[0].title == "Metroid");

    const auto byTitle = iris::LibraryService::query(library, "", iris::LibrarySort::Title);
    REQUIRE(byTitle.size() == 3);
    REQUIRE(byTitle[0].title == "Marble");
    REQUIRE(byTitle[2].title == "Zelda");

    const auto byPlayed = iris::LibraryService::query(library, "", iris::LibrarySort::LastPlayed);
    REQUIRE(byPlayed[0].title == "Metroid");
    REQUIRE(byPlayed[1].title == "Zelda");

    const auto recent = iris::LibraryService::recent(library, 1);
    REQUIRE(recent.size() == 1);
    REQUIRE(recent[0].title == "Metroid");

    iris::LibraryEntry again = game("b.nes", "Metroid", 0);
    again.addedUnixMs = 99;
    REQUIRE(iris::LibraryService::upsert(library, again, error));
    REQUIRE(library.games.size() == 3);
    const auto kept = iris::LibraryService::query(library, "met", iris::LibrarySort::Title);
    REQUIRE(kept[0].addedUnixMs == 10);
    REQUIRE(kept[0].lastPlayedUnixMs == 30);
}

TEST_CASE("version 1 and 2 configuration migrates and version 3 keeps audio and keys") {
    const auto version1 = iris::ConfigService::fromJson(
        R"({"version":1,"ui":{"theme":"xp-blue","windowWidth":1024,"windowHeight":768},"logging":{"level":"info"}})");
    REQUIRE(version1.ok());
    REQUIRE(version1.value().version == 7);
    REQUIRE(version1.value().input.turboA == 4);
    REQUIRE(version1.value().input.turboB == 22);
    REQUIRE(version1.value().input.fastForward == 66);
    REQUIRE(version1.value().input.screenshot == 69);
    REQUIRE(version1.value().volume == 1.0f);
    REQUIRE(version1.value().showLibrary);
    REQUIRE(version1.value().input.a == 29);
    REQUIRE_FALSE(iris::bindingConflict(version1.value().input).has_value());

    const auto version2 = iris::ConfigService::fromJson(
        R"({"version":2,"ui":{"theme":"xp-blue","windowWidth":1024,"windowHeight":768},"logging":{"level":"info"},"video":{"scaleMode":"integer","integerScale":2,"aspectRatio":"4:3","filter":"crt","fullscreen":false,"vsync":true,"showPerformance":false,"brightness":0,"contrast":1,"saturation":1,"gamma":1,"sharpness":2,"scanline":0.5}})");
    REQUIRE(version2.ok());
    REQUIRE(version2.value().version == 7);
    REQUIRE(version2.value().scaleMode == iris::ScaleMode::Integer);
    REQUIRE(version2.value().integerScale == 2);
    REQUIRE(version2.value().audioDevice.empty());
    REQUIRE(version2.value().audioLatency == iris::AudioLatency::Balanced);

    iris::Config config = iris::ConfigService::defaults();
    config.volume = 0.5f;
    config.muted = true;
    config.audioDevice = "Speakers";
    config.audioLatency = iris::AudioLatency::Long;
    config.input.a = 6;
    config.input.b = 7;
    config.showLibrary = false;
    const auto roundTrip = iris::ConfigService::fromJson(iris::ConfigService::toJson(config));
    REQUIRE(roundTrip.ok());
    REQUIRE(roundTrip.value() == config);

    const auto incomplete = iris::ConfigService::fromJson(
        R"({"version":3,"ui":{"theme":"xp-blue","windowWidth":1024,"windowHeight":768},"logging":{"level":"info"},"video":{"scaleMode":"fit","integerScale":3,"aspectRatio":"pixel","filter":"nearest","fullscreen":false,"vsync":true,"showPerformance":false,"brightness":0,"contrast":1,"saturation":1,"gamma":1,"sharpness":2,"scanline":0.5}})");
    REQUIRE_FALSE(incomplete.ok());

    const auto loud = iris::ConfigService::fromJson(
        R"({"version":3,"ui":{"theme":"xp-blue","windowWidth":1024,"windowHeight":768},"logging":{"level":"info"},"video":{"scaleMode":"fit","integerScale":3,"aspectRatio":"pixel","filter":"nearest","fullscreen":false,"vsync":true,"showPerformance":false,"brightness":0,"contrast":1,"saturation":1,"gamma":1,"sharpness":2,"scanline":0.5},"audio":{"volume":2,"muted":false,"device":"","latency":"balanced"},"input":{"a":29,"b":27,"select":229,"start":40,"up":82,"down":81,"left":80,"right":79,"openGame":18,"quickSave":62,"quickLoad":64,"pause":41,"fullscreen":40,"reset":21},"library":{"show":true}})");
    REQUIRE_FALSE(loud.ok());
}

TEST_CASE("controller bindings reject duplicate keys and allow Alt+Enter beside Start") {
    iris::InputBindings bindings;
    REQUIRE_FALSE(iris::bindingConflict(bindings).has_value());
    bindings.b = bindings.a;
    const auto samePad = iris::bindingConflict(bindings);
    REQUIRE(samePad.has_value());
    REQUIRE(samePad->find("A") != std::string::npos);

    bindings = iris::InputBindings{};
    bindings.pause = bindings.quickSave;
    REQUIRE(iris::bindingConflict(bindings).has_value());

    bindings = iris::InputBindings{};
    bindings.pause = bindings.a;
    REQUIRE(iris::bindingConflict(bindings).has_value());

    bindings = iris::InputBindings{};
    bindings.reset = bindings.openGame;
    const auto sameChord = iris::bindingConflict(bindings);
    REQUIRE(sameChord.has_value());
    REQUIRE(sameChord->find("Open") != std::string::npos);

    bindings = iris::InputBindings{};
    bindings.fullscreen = bindings.start;
    REQUIRE_FALSE(iris::bindingConflict(bindings).has_value());

    bindings = iris::InputBindings{};
    bindings.turboA = bindings.a;
    const auto sameTurbo = iris::bindingConflict(bindings);
    REQUIRE(sameTurbo.has_value());
    REQUIRE(sameTurbo->find("Turbo A") != std::string::npos);

    bindings = iris::InputBindings{};
    bindings.screenshot = bindings.pause;
    const auto sameShot = iris::bindingConflict(bindings);
    REQUIRE(sameShot.has_value());
    REQUIRE(sameShot->find("Screenshot") != std::string::npos);

    REQUIRE(iris::applyTurbo(iris::kButtonB, iris::kButtonA, true) == (iris::kButtonA | iris::kButtonB));
    REQUIRE(iris::applyTurbo(iris::kButtonB, iris::kButtonA, false) == iris::kButtonB);
}
